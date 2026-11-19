"""Count public Unity class IDs in loose, v22, type-tree-stripped player files."""

import argparse
import json
import re
import struct
import sys
from collections import Counter
from pathlib import Path


HEADER = struct.Struct(">IIII B 3x I Q Q Q")
METADATA_LIMIT = 16 * 1024 * 1024
SCENE_FILES = {"globalgamemanagers", "level0", "level1", "level2", "level3"}
NAME_MARKERS = {
    "combat": ("attack", "dodge", "evade", "parry", "skill", "combo", "攻击", "闪避", "技能"),
    "movement": ("idle", "walk", "sprint", "jump", "待机", "行走", "奔跑"),
    "interface": ("button", "canvas", "panel", "loading", "login", "popup", "界面", "登录"),
}
# Each space-separated pattern requires whole tokens in the same class name.
SCRIPT_MARKERS = {
    "skill_action": ("skill action",), "skill_frame": ("skill frame",),
    "skill_status": ("skill status",), "combo": ("combo", "attack chain"),
    "cancel_window": ("attack cancel", "skill cancel", "cancel window", "cancel frame"),
    "dodge": ("dodge", "evade", "evasion"),
    "invulnerability": ("invulnerability", "invulnerable", "invincible", "invincibility"),
    "signal_ball": ("signal orb", "signal ball", "skill orb", "skill ball"),
    "three_ping": ("three ping", "triple ping", "three ball", "triple ball"),
    "supercomputing": ("supercomputing",), "supercomputing_state": ("supercomputing state",),
    "time_dilation": ("time dilation",), "time_scale": ("time scale",),
    "fight_collider": ("fight collider", "battle collider", "attack collision"),
    "be_hit": ("be hit", "hit reaction"), "root_motion": ("root motion",),
    "enemy_ai": ("enemy ai", "monster ai", "npc ai"),
    "behavior_tree": ("behavior tree", "behaviour tree"),
    "fight_camera": ("fight camera", "battle camera", "combat camera"),
    "hit_camera_track": ("hit camera track",),
    "combat_ui": ("ui fight", "ui battle", "hud fight", "hud battle", "hud combat"),
}
WORDS = re.compile(r"[A-Z]+(?=[A-Z][a-z]|[^A-Za-z]|$)|[A-Z]?[a-z]+|[0-9]+")


class Directory:
    def __init__(self, data, byte_order):
        self.data = data
        self.byte_order = byte_order
        self.pos = 0

    def skip(self, size):
        if self.pos + size > len(self.data):
            raise ValueError("unexpected end of metadata")
        self.pos += size

    def read(self, fmt):
        fields = struct.Struct(self.byte_order + fmt)
        start = self.pos
        self.skip(fields.size)
        return fields.unpack_from(self.data, start)

    def string(self):
        end = self.data.find(b"\0", self.pos, self.pos + 128)
        if end == -1:
            raise ValueError("missing engine version terminator")
        version = self.data[self.pos:end].decode("ascii")
        self.pos = end + 1
        return version

    def count(self, minimum_size):
        (count,) = self.read("i")
        if not 0 <= count <= (len(self.data) - self.pos) // minimum_size:
            raise ValueError("invalid directory entry count")
        return count


def read_string(stream, limit, byte_order):
    if stream.tell() + 4 > limit:
        raise ValueError("truncated name metadata")
    (size,) = struct.unpack(byte_order + "i", stream.read(4))
    if not 0 <= size <= min(limit - stream.tell(), 4096):
        raise ValueError("unsupported name length")
    value = stream.read(size).decode("utf-8")
    padding = (-stream.tell()) % 4
    if stream.tell() + padding > limit:
        raise ValueError("name outside object")
    stream.seek(padding, 1)
    return value


def payload_category(prefix):
    if not prefix:
        return "empty"
    if prefix.startswith((b"PK\x03\x04", b"\x1f\x8b")):
        return "archive_signature"
    if prefix.startswith((b"\xff\xfe", b"\xfe\xff")):
        return "utf16_bom"
    try:
        text = prefix.decode("utf-8-sig").lstrip()
    except UnicodeDecodeError:
        return "binary_or_other_encoding"
    if any(not char.isprintable() and char not in "\t\r\n" for char in text):
        return "binary_or_other_encoding"
    if text.startswith(("{", "[")):
        return "json_like_prefix"
    if text.startswith("<"):
        return "markup_like_prefix"
    return "text_like_prefix"


def json_field_category(data):
    try:
        root = json.loads(data)
    except (ValueError, UnicodeDecodeError, RecursionError):
        return "unconfirmed_json"
    if not isinstance(root, dict):
        return "json_non_object"
    keys = set(root)
    for child in root.values():
        if isinstance(child, dict):
            keys.update(child)
    text = " ".join(key.casefold() for key in keys)
    if any(term in text for term in ("hitbox", "skillid", "attackframe", "cooldown", "signalball")):
        return "combat_field_marker"
    if any(term in text for term in ("render", "graphics", "device")):
        return "graphics_or_device_field_marker"
    return "no_selected_field_marker"


def summarize_names(path, ranges, byte_order):
    clips, buckets, markers = Counter(), Counter(), {}
    text_names, text_formats, json_fields = Counter(), Counter(), Counter()
    with path.open("rb") as stream:
        for class_id, offset, object_size in ranges:
            stream.seek(offset)
            limit = offset + object_size
            name = read_string(stream, limit, byte_order).casefold()
            if class_id == 49:
                if stream.tell() + 4 > limit:
                    raise ValueError("truncated TextAsset length")
                (size,) = struct.unpack(byte_order + "i", stream.read(4))
                if not 0 <= size <= limit - stream.tell():
                    raise ValueError("invalid TextAsset length")
                words = ("battle", "fight", "combat", "skill", "attack", "dodge", "orb", "hitbox",
                         "战斗", "技能", "攻击", "闪避", "信号球")
                category = "combat_word" if any(word in name for word in words) else "unclassified"
                text_names[category] += 1
                prefix = stream.read(min(size, 128))
                kind = payload_category(prefix)
                text_formats[kind] += 1
                if kind == "json_like_prefix" and size <= 65536:
                    data = prefix + stream.read(size - len(prefix))
                    json_fields[json_field_category(data)] += 1
                continue
            if class_id == 74:
                matches = [group for group, words in NAME_MARKERS.items()
                           if any(word in name for word in words)]
                clips.update(matches or ["unclassified"])
                continue
            # Player MonoScript layout for Unity 2022.3: execution order and property hash.
            stream.seek(4 + 16, 1)
            class_name = read_string(stream, limit, byte_order)
            namespace = read_string(stream, limit, byte_order)
            assembly = read_string(stream, limit, byte_order).casefold().removesuffix(".dll")
            if assembly.startswith(("unity.", "unityengine", "cinemachine", "system.")):
                bucket = "engine_package"
            elif assembly == "assembly-csharp":
                bucket = "default_player_assembly"
            else:
                bucket = "other_assembly"
            buckets[bucket] += 1
            tokens = {word.lower() for word in WORDS.findall(class_name)}
            namespace_tokens = {word.lower() for word in WORDS.findall(namespace)}
            ui_hint = {"ui", "hud"}.intersection(tokens | namespace_tokens)
            for group, patterns in SCRIPT_MARKERS.items():
                if any(tokens.issuperset(pattern.split()) for pattern in patterns):
                    context = bucket + ("/ui_name_or_namespace" if ui_hint else "/other_context")
                    markers.setdefault(group, Counter())[context] += 1
    return {"animation_clip_name_labels": clips, "text_asset_name_labels": text_names,
            "text_asset_payload_prefixes": text_formats, "text_asset_json_fields": json_fields,
            "mono_script_assemblies": buckets,
            "mono_script_name_labels": dict(sorted(markers.items()))}


def inspect_file(path, classify_names=False):
    file_size = path.stat().st_size
    with path.open("rb") as stream:
        header = stream.read(HEADER.size)
        if len(header) != HEADER.size:
            raise ValueError("incomplete header")
        _, _, version, _, endian, size, declared_size, objects_start, _ = HEADER.unpack(header)
        if version != 22 or endian not in (0, 1):
            raise ValueError("unsupported serialized file header")
        if declared_size != file_size or not 0 < size <= METADATA_LIMIT:
            raise ValueError("unsupported file or metadata size")
        if not HEADER.size + size <= objects_start <= file_size:
            raise ValueError("invalid metadata range")
        metadata = stream.read(size)
        if len(metadata) != size:
            raise ValueError("incomplete directory")

    directory = Directory(metadata, "<" if endian == 0 else ">")
    engine = directory.string()
    platform, has_type_tree = directory.read("iB")
    if has_type_tree != 0:
        raise ValueError("only stripped type trees are supported")
    classes = []
    for _ in range(directory.count(23)):
        class_id, _, _ = directory.read("iBh")
        if class_id == 114:  # MonoBehaviour types have a script ID as well.
            directory.skip(16)
        directory.skip(16)  # Type hash.
        classes.append(class_id)

    counts = Counter()
    name_ranges = []
    for _ in range(directory.count(24)):
        directory.skip((-directory.pos) % 4)
        _, relative_start, object_size, type_index = directory.read("qQIi")
        if not 0 <= type_index < len(classes):
            raise ValueError("invalid object type index")
        if objects_start + relative_start + object_size > file_size:
            raise ValueError("invalid object range")
        class_id = classes[type_index]
        counts[class_id] += 1
        if classify_names and class_id in (49, 74, 115):
            name_ranges.append((class_id, objects_start + relative_start, object_size))
    report = {
        "engine": engine,
        "format": version,
        "platform_id": platform,
        "type_entries": len(classes),
        "objects": sum(counts.values()),
        "class_counts": dict(sorted(counts.items())),
    }
    if classify_names:
        if not engine.startswith("2022.3."):
            raise ValueError("name classification only supports 2022.3 player objects")
        report["names"] = summarize_names(path, name_ranges, directory.byte_order)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("data_directory", type=Path, help="player PGR_Data directory")
    parser.add_argument("--classify-names", action="store_true",
                        help="classify names and TextAsset format prefixes (counts only)")
    args = parser.parse_args()
    root = args.data_directory
    if not root.is_dir():
        parser.error("provide a directory")
    files = sorted(p for p in root.iterdir() if p.is_file() and
                   (p.name in SCENE_FILES or p.suffix.lower() == ".assets"))
    if not files:
        parser.error("no eligible loose files at the directory root")
    reports = {}
    failed = False
    for path in files:
        try:
            reports[path.name] = inspect_file(path, args.classify_names)
        except (OSError, ValueError, struct.error) as exc:
            reports[path.name] = {"skipped": str(exc)}
            failed = True
    print(json.dumps(reports, indent=2))
    return int(failed)


if __name__ == "__main__":
    sys.exit(main())
