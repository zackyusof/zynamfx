#!/usr/bin/env python3
"""Build the zynamfx standalone app, AU, and VST3 plug-in."""
import argparse
from datetime import datetime
from pathlib import Path
import os
import shutil
import subprocess


def prune_old_backups(destination):
    """Delete older `-backup-` copies of this bundle so repeated --install runs
    leave only the newest one behind (the one about to be made)."""
    for old in destination.parent.glob(destination.stem + "-backup-*" + destination.suffix):
        if old.is_dir():
            shutil.rmtree(old, ignore_errors=True)
        else:
            old.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--install", action="store_true", help="install app, AU, and VST3 to their standard user locations")
    parser.add_argument("--public", action="store_true",
                        help="public (GitHub) channel: the in-app logo and Settings footer point to "
                             "github.com/zackyusof/zynamfx. Default is the development (Gitea) channel.")
    parser.add_argument("--jobs", type=int, default=min(8, os.cpu_count() or 4))
    args = parser.parse_args()
    source = Path(__file__).resolve().parent.parent
    # Which repository this build belongs to. Development is built from Gitea and
    # points there; the public build is cloned to GitHub and points there. The
    # URL is baked in (logo link + Settings footer) so the two never mix.
    if args.public:
        repo_url, channel = "https://github.com/zackyusof/zynamfx", "public"
    else:
        repo_url, channel = "https://git.zyusof.net/zack/zynamfx", "development"
    print(f"Building the {channel} channel from {repo_url}")
    # Keep the build underneath source: JUCE generated AppIcon.icns otherwise
    # falls outside plugin/CMakeLists.txt source_group(TREE ...) and configure fails.
    build = source / "build"
    cmake = shutil.which("cmake")
    if not cmake or not shutil.which("ninja"):
        parser.error("cmake and ninja must be available on PATH")
    if (source / ".git").exists():
        subprocess.run(["git", "submodule", "update", "--init", "--recursive"], cwd=source, check=True)
    elif not (source / "plugin/NeuralAmpModelerCore/CMakeLists.txt").is_file():
        parser.error("Source archive is missing its vendored dependencies")
    subprocess.run([cmake, "-S", str(source), "-B", str(build), "-G", "Ninja",
                    "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
                    "-DT3K_FORK_NAMESPACED=ON", "-DT3K_STANDALONE_ONLY=OFF",
                    "-DT3K_DESKTOP_BUNDLE_ID=net.zyusof.zynamfx",
                    "-DT3K_FORK_PRODUCT_NAME=zynamfx",
                    "-DT3K_FORK_DISPLAY_NAME=zynamfx",
                    "-DT3K_FORK_DATA_DIR=zynamfx",
                    f"-DT3K_FORK_REPO_URL={repo_url}",
                    f"-DT3K_FORK_BUILD_CHANNEL={channel}",
                    "-DT3K_PUBLISHABLE_KEY=",
                    "-DT3K_API_DOMAIN=https://www.tone3000.com",
                    "-DT3K_UPDATE_NOTICE=false",
                    "-DBUILD_AAX=OFF", "-DBUILD_LV2=OFF", "-DBUILD_CLAP=OFF", "-DBUILD_VST3=ON"], check=True)
    config = (build / "t3k_ui/T3kConfig.h").read_text()
    if ('kPublishableKey = "";' not in config or
            'kApiOrigin = "https://www.tone3000.com";' not in config or
            "kUpdateNoticeEnabled = 0;" not in config):
        raise RuntimeError("Public build configuration is not keyless or uses an unexpected service setting")
    subprocess.run(["sh", str(source / "script/patch_juce_popupmenu.sh")], check=True)
    subprocess.run([cmake, "--build", str(build), "--target", "TONE3000_Standalone", "TONE3000_AU", "TONE3000_VST3",
                    "-j", str(args.jobs)], check=True)
    app = build / "plugin/TONE3000_artefacts/Release/Standalone/zynamfx.app"
    au = build / "plugin/TONE3000_artefacts/Release/AU/zynamfx.component"
    vst3 = build / "plugin/TONE3000_artefacts/Release/VST3/zynamfx.vst3"
    if not (app / "Contents/MacOS/zynamfx").is_file() or not (au / "Contents/Info.plist").is_file() or not (vst3 / "Contents/Info.plist").is_file():
        raise RuntimeError("Standalone app, AU, or VST3 bundle missing")
    # Preserve upstream notices in all delivered bundles.
    notices = [
        ("LICENSE", "TONE3000-MIT.txt"),
        ("COPYING", "GNU-AGPLv3.txt"),
        ("COPYING.GPL", "GNU-GPLv3.txt"),
        ("LICENSE-SCOPE.md", "LICENSE-SCOPE.md"),
        ("SOURCE.md", "SOURCE.md"),
        ("plugin/NeuralAmpModelerCore/LICENSE", "NeuralAmpModelerCore-MIT.txt"),
        ("plugin/AudioDSPTools/LICENSE", "AudioDSPTools-MIT.txt"),
        ("libs/juce/LICENSE.md", "JUCE.md"),
        ("plugin/ui/assets/Arimo-LICENSE.txt", "Arimo.txt"),
        ("plugin/ui/assets/RobotoMono-LICENSE.txt", "RobotoMono.txt"),
        ("plugin/NeuralAmpModelerCore/Dependencies/eigen/COPYING.MPL2", "Eigen-MPL2.txt"),
    ]
    for bundle in (app, au, vst3):
        licenses = bundle / "Contents/Resources/Licenses"
        licenses.mkdir(parents=True, exist_ok=True)
        for relative, name in notices:
            shutil.copy2(source / relative, licenses / name)
    for tree in ["libs/juce/modules", "plugin/NeuralAmpModelerCore", "plugin/AudioDSPTools", "plugin/ui/vendor"]:
        for notice in (source / tree).rglob("*"):
            if notice.is_file() and notice.name.upper().startswith(("LICENSE", "LICENCE", "COPYING", "NOTICE", "COPYRIGHT")):
                relative = notice.relative_to(source)
                for bundle in (app, au, vst3):
                    target = bundle / "Contents/Resources/Licenses/components" / relative
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(notice, target)
    for bundle in (app, au, vst3):
        shutil.copy2(source / "plugin/ui/assets/Lucide-LICENSE.txt", bundle / "Contents/Resources/Licenses/Lucide.txt")
    # The linker signs the executable, but a local JUCE app build does not
    # necessarily seal the bundle resources. Seal the complete test bundle.
    for bundle in (app, au, vst3):
        subprocess.run(["codesign", "--force", "--deep", "--sign", "-", str(bundle)], check=True)
        subprocess.run(["codesign", "--verify", "--deep", "--strict", str(bundle)], check=True)
    if args.install:
        # Install the app to /Applications (the standard macOS location), or to
        # ~/Applications when that is not writable (no admin rights).
        apps = Path("/Applications")
        if not os.access(str(apps), os.W_OK):
            apps = Path.home() / "Applications"
        apps.mkdir(parents=True, exist_ok=True)
        destination = apps / app.name
        if destination.exists():
            prune_old_backups(destination)
            backup = destination.with_name(destination.stem + "-backup-" + datetime.now().strftime("%Y%m%d-%H%M%S") + ".app")
            shutil.copytree(destination, backup, symlinks=True)
            if sum(1 for p in backup.rglob("*") if p.is_file()) != sum(1 for p in destination.rglob("*") if p.is_file()):
                raise RuntimeError("Backup file count mismatch")
            shutil.rmtree(destination)
        shutil.copytree(app, destination, symlinks=True)
        subprocess.run(["xattr", "-dr", "com.apple.quarantine", str(destination)], check=True)
        subprocess.run(["codesign", "--verify", "--deep", "--strict", str(destination)], check=True)
        au_destination = Path.home() / "Library/Audio/Plug-Ins/Components" / au.name
        au_destination.parent.mkdir(parents=True, exist_ok=True)
        if au_destination.exists():
            prune_old_backups(au_destination)
            backup = au_destination.with_name(au_destination.stem + "-backup-" + datetime.now().strftime("%Y%m%d-%H%M%S") + ".component")
            shutil.copytree(au_destination, backup, symlinks=True)
            if sum(1 for p in backup.rglob("*") if p.is_file()) != sum(1 for p in au_destination.rglob("*") if p.is_file()):
                raise RuntimeError("AU backup file count mismatch")
            shutil.rmtree(au_destination)
        shutil.copytree(au, au_destination, symlinks=True)
        subprocess.run(["xattr", "-dr", "com.apple.quarantine", str(au_destination)], check=True)
        subprocess.run(["codesign", "--verify", "--deep", "--strict", str(au_destination)], check=True)
        vst3_destination = Path.home() / "Library/Audio/Plug-Ins/VST3" / vst3.name
        vst3_destination.parent.mkdir(parents=True, exist_ok=True)
        if vst3_destination.exists():
            prune_old_backups(vst3_destination)
            backup = vst3_destination.with_name(vst3_destination.stem + "-backup-" + datetime.now().strftime("%Y%m%d-%H%M%S") + ".vst3")
            shutil.copytree(vst3_destination, backup, symlinks=True)
            if sum(1 for p in backup.rglob("*") if p.is_file()) != sum(1 for p in vst3_destination.rglob("*") if p.is_file()):
                raise RuntimeError("VST3 backup file count mismatch")
            shutil.rmtree(vst3_destination)
        shutil.copytree(vst3, vst3_destination, symlinks=True)
        subprocess.run(["xattr", "-dr", "com.apple.quarantine", str(vst3_destination)], check=True)
        subprocess.run(["codesign", "--verify", "--deep", "--strict", str(vst3_destination)], check=True)
        print(destination)
        print(au_destination)
        print(vst3_destination)
    else:
        print(app)
        print(au)
        print(vst3)


if __name__ == "__main__":
    main()
