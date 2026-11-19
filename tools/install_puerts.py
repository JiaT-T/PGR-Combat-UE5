"""Install the project's pinned upstream Puerts + QuickJS profile for Win64."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


UPSTREAM = "https://github.com/Tencent/puerts.git"
REVISION = "46ced3d44b1faea7ad2e3511497bbfb369824be2"
MODULES = ("ScriptEnv", "PapiQuickjs", "ParamDefaultValueMetas")
QUICKJS = Path("Source/PapiQuickjs/Private/quickjs/quickjs.c")
QUICKJS_SHA256 = "2671049fab412b4ebb0846d15556a4d854ee2c2c71d7081546d4b3a373f251eb"


# 在指定目录运行 Git 命令并返回标准输出。
def git(checkout: Path, *args: str) -> str:
    result = subprocess.run(
        ["git", "-C", str(checkout), *args], check=True, text=True,
        stdout=subprocess.PIPE,
    )
    return result.stdout.strip()


# 校验固定版本的 Puerts 和 QuickJS，并安装或核对本地插件。
def install(destination: Path) -> None:
    destination = destination.resolve()
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="puerts-install-", dir=destination.parent) as stage:
        work = Path(stage)
        checkout = work / "source"
        checkout.mkdir()
        git(checkout, "init", "--quiet")
        git(checkout, "config", "core.autocrlf", "true")
        git(checkout, "config", "core.longpaths", "true")
        git(checkout, "remote", "add", "origin", UPSTREAM)
        git(checkout, "config", "remote.origin.promisor", "true")
        git(checkout, "config", "remote.origin.partialclonefilter", "blob:none")
        git(checkout, "sparse-checkout", "set", "--cone", "unreal/Puerts")
        git(checkout, "fetch", "--quiet", "--depth=1", "--filter=blob:none", "origin", REVISION)
        git(checkout, "checkout", "--quiet", "--detach", "FETCH_HEAD")
        if git(checkout, "rev-parse", "HEAD") != REVISION:
            raise RuntimeError("Unexpected upstream commit")

        candidate = work / "Puerts"
        shutil.copytree(checkout / "unreal/Puerts", candidate)
        shutil.copy2(checkout / "LICENSE", candidate / "LICENSE")
        descriptor = candidate / "Puerts.uplugin"
        config = json.loads(descriptor.read_text(encoding="utf-8"))
        config["Modules"] = [m for m in config["Modules"] if m["Name"] in MODULES]
        if tuple(m["Name"] for m in config["Modules"]) != MODULES:
            raise RuntimeError("Unexpected upstream module descriptors")
        descriptor.write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8", newline="\r\n")
        if hashlib.sha256((candidate / QUICKJS).read_bytes()).hexdigest() != QUICKJS_SHA256:
            raise RuntimeError("QuickJS source does not match the pinned Win64 checkout")

        if destination.exists():
            for source in candidate.rglob("*"):
                if source.is_file():
                    relative = source.relative_to(candidate)
                    target = destination / relative
                    if not target.is_file() or source.read_bytes() != target.read_bytes():
                        raise RuntimeError(f"Existing plugin differs at {relative}. No files replaced.")
            print(f"Verified existing Puerts at {destination}")
        else:
            candidate.rename(destination)
            print(f"Installed Puerts {REVISION} with QuickJS at {destination}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path, nargs="?", default=Path("Plugins/Puerts"))
    install(parser.parse_args().destination)
