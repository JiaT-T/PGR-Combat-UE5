"""Report aggregate metadata from a local PGR Windows install. Stdlib only."""

import argparse
import json
import re
from collections import Counter
from pathlib import Path, PurePosixPath


RESOURCE_BASE = "PGR_Data/StreamingAssets/resource"
GROUPS = ("launch", "matrix")
SAMPLE_SIZE = 16
SIGNATURES = (
    (b"UnityFS\0", "UnityFS"),
    (b"HARUINKURO", "HARUINKURO prefix"),
    (b"@UTF", "@UTF"),
    (b"AFS2", "AFS2"),
    (b"CRID", "CRID"),
)


def inspect_group(root, group, records):
    prefix = f"{RESOURCE_BASE}/{group}/"
    expected = {
        row["dest"][len(prefix):]: row["size"]
        for row in records
        if row["dest"].startswith(prefix) and "/" not in row["dest"][len(prefix):]
    }
    folder = root / RESOURCE_BASE / group
    sizes = {}
    by_ext = {}
    for path in folder.iterdir():
        if path.is_symlink() or not path.is_file():
            continue
        sizes[path.name] = path.stat().st_size
        by_ext.setdefault(path.suffix or "(none)", []).append(path)

    groups = {}
    for extension, paths in sorted(by_ext.items()):
        paths.sort(key=lambda item: item.name)
        # Deterministic, evenly spaced positions in sorted names, inclusive of ends.
        n = min(SAMPLE_SIZE, len(paths))
        headers = Counter()
        for i in range(n):
            path = paths[i * (len(paths) - 1) // max(1, n - 1)]
            with path.open("rb") as stream:
                prefix_bytes = stream.read(16)
            label = next((label for magic, label in SIGNATURES
                          if prefix_bytes.startswith(magic)), "other")
            headers[label] += 1
        groups[extension] = {
            "files": len(paths),
            "bytes": sum(sizes[path.name] for path in paths),
            "sample_headers": dict(headers),
        }
    return {
        "manifest_unique_names": len(expected),
        "missing_on_disk": len(expected.keys() - sizes.keys()),
        "extra_on_disk": len(sizes.keys() - expected.keys()),
        "size_mismatches": sum(sizes[k] != expected[k] for k in expected.keys() & sizes.keys()),
        "extensions": groups,
    }


def inspect(root):
    data = root / "PGR_Data"
    records = json.loads((root / "LocalGameResources.json").read_text(encoding="utf-8-sig"))["resource"]
    version_text = (root / "version.json").read_text(encoding="utf-8-sig")
    package = re.search(r'package_version["\s]*:\s*"?([\d.]+)', version_text)
    with (data / "globalgamemanagers").open("rb") as stream:
        serialized_header = stream.read(96)
    match = re.search(rb"\d{4}\.\d+\.\d+[a-z]\d+(?:c\d+)?\x00", serialized_header)
    assemblies = set(json.loads((data / "ScriptingAssemblies.json").read_text(encoding="utf-8-sig"))["names"])
    selected = ("Cinemachine.dll", "Unity.Timeline.dll", "Unity.AI.Navigation.dll",
                "UnityEngine.AnimationModule.dll", "UnityEngine.PhysicsModule.dll", "UnityEngine.UI.dll")
    return {
        "package_version": package.group(1) if package else "unrecognized",
        "serialized_unity_version": match.group().rstrip(b"\0").decode() if match else "unrecognized",
        "runtime_files_present": {
            name: (root / name).is_file() for name in ("PGR.exe", "UnityPlayer.dll", "GameAssembly.dll")
        },
        "metadata_dat_bytes": (data / "il2cpp_data/Metadata/global-metadata.dat").stat().st_size,
        "selected_listed_assemblies": [name for name in selected if name in assemblies],
        "manifest_rows": len(records),
        "manifest_extensions": dict(Counter(PurePosixPath(r["dest"]).suffix or "(none)" for r in records)),
        "resource_folders": {group: inspect_group(root, group, records) for group in GROUPS},
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("client_root", type=Path, help="Folder containing PGR.exe")
    args = parser.parse_args()
    try:
        result = inspect(args.client_root)
    except (OSError, ValueError, TypeError, KeyError) as error:
        parser.exit(1, f"Unsupported or unavailable installation: {error}\n")
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
