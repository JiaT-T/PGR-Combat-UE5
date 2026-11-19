"""Summarize two standard UnityFS indexes. Print aggregate metadata only."""

import argparse
from collections import Counter
import json
from pathlib import Path
import re
import struct


# 遇到不支持的索引布局时报告原因并停止解析。
def require(condition, message):
    if not condition:
        raise ValueError(message)


# 将偏移量向上对齐到指定字节边界。
def align(value, boundary):
    return (value + boundary - 1) // boundary * boundary


# 解压一个标准 LZ4 数据块并校验输出大小。
def lz4_block(data, expected):
    # Unity format 3 (LZ4HC) uses the standard LZ4 block representation.
    require(0 <= expected <= 256 * 1024, "Unexpected block size")
    out = bytearray()
    p = 0

    # 读取 LZ4 长度扩展字节，并推进当前块的读取位置。
    def length(base):
        nonlocal p
        if base == 15:
            while p < len(data):
                extra = data[p]
                p += 1
                base += extra
                if extra < 255:
                    break
        return base

    while p < len(data):
        token = data[p]
        p += 1
        literals = length(token >> 4)
        require(p + literals <= len(data) and len(out) + literals <= expected, "Invalid literals")
        out.extend(data[p:p + literals])
        p += literals
        if p == len(data):
            break
        offset, = struct.unpack_from("<H", data, p)
        p += 2
        count = length(token & 15) + 4
        require(0 < offset <= len(out) and len(out) + count <= expected, "Invalid match")
        pattern = out[-offset:][:count]
        repeats = (count + len(pattern) - 1) // len(pattern)
        out.extend((pattern * repeats)[:count])
    require(len(out) == expected, "LZ4 size mismatch")
    return out


# 校验 UnityFS 索引包布局并读取其中的序列化文件。
def read_index_bundle(path):
    require(path.stat().st_size <= 16 * 1024 * 1024, "Index exceeds supported size")
    data = path.read_bytes()
    require(data.startswith(b"UnityFS\x00\x00\x00\x00\x08"), "Expected UnityFS v8")
    engine_start = data.index(0, 12) + 1
    engine_end = data.index(0, engine_start)
    engine = data[engine_start:engine_end].decode("ascii")
    total, packed, unpacked, flags = struct.unpack_from(">QIII", data, engine_end + 1)
    require(total == len(data) and flags == 0x243, "Unsupported UnityFS flags/size")
    start = align(engine_end + 21, 16)
    info = lz4_block(data[start:start + packed], unpacked)
    block_count, = struct.unpack_from(">I", info, 16)
    require(0 < block_count < 256, "Unsupported block count")
    node_offset = 20 + block_count * 10
    nodes, offset, node_size, node_flags = struct.unpack_from(">IqqI", info, node_offset)
    require((nodes, offset, node_flags) == (1, 0, 4), "Expected one serialized file")
    start = align(start + packed, 16)
    serialized = bytearray()
    for i in range(block_count):
        output_size, input_size, codec = struct.unpack_from(">IIH", info, 20 + i * 10)
        require(codec == 3 and start + input_size <= len(data), "Unexpected block layout")
        serialized.extend(lz4_block(data[start:start + input_size], output_size))
        start += input_size
    require(len(serialized) == node_size and start == len(data), "Bundle bounds mismatch")
    return serialized, engine, block_count


# 从索引的对象目录定位并读取 TextAsset 内容。
def read_text_asset(data):
    require(struct.unpack_from(">I", data, 8)[0] == 22 and data[16] == 0, "Expected little-endian serialized file v22")
    data_offset, = struct.unpack_from(">Q", data, 32)
    pos = data.index(0, 48) + 1
    _, type_tree, type_count = struct.unpack_from("<iBi", data, pos)
    require(type_tree == 1 and type_count == 2, "Unexpected type tree")
    pos += 9
    classes = []
    for _ in range(type_count):
        class_id, = struct.unpack_from("<i", data, pos)
        require(class_id in (49, 142), "Expected TextAsset/AssetBundle")
        classes.append(class_id)
        pos += 23  # Class ID, stripped flag, script index and type hash
        nodes, string_bytes = struct.unpack_from("<II", data, pos)
        pos += 8 + nodes * 32 + string_bytes
        dependencies, = struct.unpack_from("<I", data, pos)
        pos += 4 + dependencies * 4
    count, = struct.unpack_from("<I", data, pos)
    require(count == 2 and set(classes) == {49, 142}, "Unknown index schema")
    pos += 4
    text_asset = None
    object_classes = []
    for _ in range(count):
        pos = align(pos, 4)
        _, relative, size, class_index = struct.unpack_from("<qQII", data, pos)
        pos += 24
        object_classes.append(classes[class_index])
        if classes[class_index] == 49:
            start = data_offset + relative
            end = start + size
            name_size, = struct.unpack_from("<I", data, start)
            script_pos = align(start + 4 + name_size, 4)
            script_size, = struct.unpack_from("<I", data, script_pos)
            require(script_pos + 4 + script_size <= end <= len(data), "TextAsset bounds mismatch")
            text_asset = data[script_pos + 4:script_pos + 4 + script_size]
    require(sorted(object_classes) == [49, 142] and text_asset is not None, "Missing index objects")
    return text_asset


# 将 TextAsset 中的 MessagePack 索引解码为 Python 对象。
def decode_messagepack(payload):
    try:
        import msgpack
    except ImportError:
        # Offline fallback on Python installations with pip's bundled msgpack.
        from pip._vendor import msgpack
    return msgpack.unpackb(bytes(payload), raw=False, strict_map_key=False)


# 读取 CRI 表中的字段，二进制字段只记录位置和长度。
def utf_rows(data):
    """Read a plain CRI @UTF table. Return strings and offsets without reading data blobs."""
    require(data[:4] == b"@UTF", "Expected a plain @UTF table")
    row_start = 8 + struct.unpack_from(">H", data, 10)[0]
    string_start, blob_start = (8 + n for n in struct.unpack_from(">II", data, 12))
    cols, width, count = struct.unpack_from(">HHI", data, 24)
    require(cols <= 128 and count <= 4096 and blob_start <= len(data), "Unexpected table bounds")
    formats = ("B", "b", "H", "h", "I", "i", "Q", "q", "f", "d", "I", "II")
    schema = []
    pos = 32
    field_offset = 0

    # 从 CRI 字符串区的指定偏移读取 UTF-8 字符串。
    def string(offset):
        start = string_start + offset
        return data[start:data.index(0, start, blob_start)].decode("utf-8")

    for _ in range(cols):
        flags, name_offset = struct.unpack_from(">BI", data, pos)
        storage, kind = divmod(flags, 16)
        require(storage in (1, 3, 5) and kind < len(formats), "Unknown @UTF field")
        fmt = ">" + formats[kind]
        pos += 5
        schema.append((string(name_offset), fmt, kind, storage, pos if storage == 3 else field_offset))
        if storage == 3:
            pos += struct.calcsize(fmt)
        if storage == 5:
            field_offset += struct.calcsize(fmt)
    require(field_offset == width and row_start + count * width <= string_start, "Unexpected @UTF row")
    rows = []
    for i in range(count):
        row = {}
        for name, fmt, kind, storage, offset in schema:
            if storage == 1:
                continue
            address = offset if storage == 3 else row_start + i * width + offset
            values = struct.unpack_from(fmt, data, address)
            value = values[0]
            if kind == 10:
                value = string(value)
            elif kind == 11:
                value = (blob_start + value, values[1])
            row[name] = value
        rows.append(row)
    return rows


# 汇总训练视频的画面规格和选定音频表的提示音数量。
def media_metadata(directory, files):
    # Logical names are selection criteria only. Physical reads use validated basenames.
    selected = {
        "External/Criware/g_ui.acb": ("b_ui_skillsingle", "b_ui_skillthree"),
        "External/Criware/g_ingame_tongyongfight.acb": ("g_ingame_shieldbreak", "g_ingame_tongyongjiaqiu"),
        "External/Criware/e_signal.acb": ("e_signal_attack_",),
    }
    video_modes = Counter()
    cue_counts = {}
    for name, entry in files.items():
        logical = entry[0]
        training = logical.startswith("External/Video/simulatetrain") and logical.endswith(".usm")
        if logical not in selected and not training:
            continue
        require(re.fullmatch(r"[a-f0-9]{16}\.(acb|usm)", name), "Unexpected media basename")
        with (directory / name).open("rb") as stream:
            if training:
                # The first chunk identifies the container. The next holds stream parameters.
                head = stream.read(4096)
                require(head[:4] == b"CRID", "Unknown movie container")
                chunk = 8 + struct.unpack_from(">I", head, 4)[0]
                require(head[chunk:chunk + 4] == b"@SFV", "Unknown video stream")
                header = chunk + 8 + struct.unpack_from(">H", head, chunk + 8)[0]
                fields = utf_rows(head[header:])[0]
                mode = "{width}x{height} {framerate_n}/{framerate_d}".format(**fields)
                video_modes[mode] += 1
            else:
                header = stream.read(32)
                require(header[:4] == b"@UTF", "Unknown audio container")
                prefix = 8 + struct.unpack_from(">I", header, 16)[0]
                require(prefix <= 65536, "Unexpected header size")
                stream.seek(0)
                cue_offset, cue_size = utf_rows(stream.read(prefix))[0]["CueNameTable"]
                require(cue_size <= 1024 * 1024, "Unexpected cue table size")
                stream.seek(cue_offset)
                rows = utf_rows(stream.read(cue_size))
                cue_counts[Path(logical).stem] = {
                    "rows": len(rows),
                    "matching_prefixes": {
                        term: sum(r["CueName"].startswith(term) for r in rows)
                        for term in selected[logical]
                    },
                }
    return {"training_video_header_modes": dict(video_modes), "selected_cue_tables": cue_counts}


# 汇总索引中的资源类型和逻辑名，并按需读取媒体头。
def summarize(path, inspect_media=False):
    data, engine, blocks = read_index_bundle(path)
    payload = read_text_asset(data)
    root = decode_messagepack(payload)
    require(isinstance(root, list) and isinstance(root[0], dict), "Unexpected MessagePack index")
    files = root[0]
    require(all(isinstance(k, str) for k in files), "Unexpected path keys")
    extensions = Counter(Path(k).suffix.lower() for k in files)
    opaque_uab = sum(bool(re.fullmatch(r"[0-9a-f]{16}\.uab", k)) for k in files)
    statistics = {
        "index": path.name,
        "unity": engine,
        "blocks": blocks,
        "objects": {"AssetBundle": 1, "TextAsset": 1},
        "text_asset_bytes": len(payload),
        "msgpack_slots": [
            {"type": type(v).__name__, "length": len(v) if isinstance(v, (list, dict, str, bytes)) else None}
            for v in root
        ],
        "index_extensions": dict(extensions),
        "hashed_uab_keys": opaque_uab,
        "uab_with_logical_name": sum(bool(v[0]) for k, v in files.items() if k.endswith(".uab")),
        "logical_names_by_extension": dict(Counter(Path(k).suffix for k, v in files.items() if v[0])),
    }
    if inspect_media and path.name == "InPkgIndex":
        statistics.update(media_metadata(path.parent, files))
    return statistics


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("game_root", type=Path, help="Directory containing PGR.exe")
    parser.add_argument("--media-headers", action="store_true", help="Inspect plain cue tables and training video metadata")
    args = parser.parse_args()
    base = args.game_root / "PGR_Data" / "StreamingAssets" / "resource"
    for name in ("launch/index", "matrix/InPkgIndex"):
        print(json.dumps(summarize(base / name, args.media_headers), ensure_ascii=True))
