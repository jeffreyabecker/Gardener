#!/usr/bin/env python3
"""The prebuilt archives the firmware links instead of compiling two libraries.

A library whose `library.properties` declares `precompiled=full` is not compiled when an archive for
the board sits in its `src/{build.mcu}/`: arduino-cli adds that folder to the link and compiles the
sources only when there is no archive. The fallback is what makes this safe — a fresh clone, a new
toolchain or a deleted archive is slow, never broken.

An archive is only good for the flags and sources it was built from, so each carries an
`archive.json` beside it: the toolchain version, the define string, and a digest of the library's
sources. `verified` drops an archive whose manifest no longer matches, and the build that follows
compiles the sources and packs a fresh one from its own objects — which is why an archive cannot
drift from the flags, since arduino-cli made those objects with them.
"""

import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

MANIFEST = "archive.json"

# What arduino-cli will take from the command line as one `ar` invocation. The 189 NimBLE objects do
# not fit in a Windows command line, so the members go in batches well inside it.
BATCH_CHARS = 6000


def archive_path(library, name, mcu):
    """Where arduino-cli looks for a precompiled library, and what `-l` is derived from."""
    return library / "src" / mcu / f"lib{name}.a"


def digest(library, mcu):
    """Everything an archive is built from: the sources and the properties that declare it
    precompiled. The archive folder is left out, or packing would invalidate the manifest that
    describes the pack, and so is everything outside `src/` — an example or a README cannot change
    a single object file."""
    archive_dir = library / "src" / mcu
    files = [library / "library.properties"]
    files.extend(file for file in (library / "src").rglob("*")
                 if file.is_file() and archive_dir not in file.parents)
    sha = hashlib.sha256()
    for file in sorted(files):
        sha.update(file.relative_to(library).as_posix().encode())
        sha.update(b"\0")
        sha.update(file.read_bytes())
    return sha.hexdigest()


def drop(library, name, mcu):
    """Remove the archive and its manifest, so that the next build compiles the sources instead."""
    archive_dir = library / "src" / mcu
    if not archive_dir.is_dir():
        return
    for file in archive_dir.iterdir():
        if file.suffix == ".a" or file.name == MANIFEST:
            file.unlink()


def verified(library, name, mcu, toolchain, flags):
    """Whether the archive is the one these flags and sources would build, dropping it when not.

    Returns the reason it was dropped, or None when it is current or absent. An absent archive is
    not a problem: the build compiles the sources and packs afterwards.
    """
    archive = archive_path(library, name, mcu)
    if not archive.exists():
        return None
    try:
        recorded = json.loads((archive.parent / MANIFEST).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        recorded = {}
    if not recorded:
        reason = "no manifest"
    elif recorded.get("toolchain") != toolchain:
        reason = f"packed by toolchain {recorded['toolchain']}"
    elif recorded.get("flags") != flags:
        reason = "packed with other flags"
    elif recorded.get("digest") != digest(library, mcu):
        reason = "the sources changed"
    else:
        return None
    drop(library, name, mcu)
    return reason


def pack(library, name, mcu, objects, ar, toolchain, flags):
    """Archive a build's own object files, and record what they were built with."""
    archive = archive_path(library, name, mcu)
    archive.parent.mkdir(parents=True, exist_ok=True)
    drop(library, name, mcu)
    # `ar` names a member after the file it was given and nothing else, so two objects called
    # `transport.c.o` in different folders would replace each other and one would leave the archive
    # without a word. The copies are flat, named after the whole path, and are only what `ar` reads.
    staging = objects.parent / f"{objects.name}.pack"
    shutil.rmtree(staging, ignore_errors=True)
    staging.mkdir(parents=True)
    members = []
    for object_file in sorted(objects.rglob("*.o")):
        member = object_file.relative_to(objects).as_posix().replace("/", "_")
        shutil.copy2(object_file, staging / member)
        members.append(member)
    batch = []
    for member in members:
        if sum(len(entry) + 1 for entry in batch) + len(member) > BATCH_CHARS:
            _add(ar, archive, staging, batch)
            batch = []
        batch.append(member)
    if batch:
        _add(ar, archive, staging, batch)
    shutil.rmtree(staging, ignore_errors=True)
    (archive.parent / MANIFEST).write_text(
        json.dumps({"toolchain": toolchain, "flags": flags, "digest": digest(library, mcu)},
                   indent=2) + "\n", encoding="utf-8")
    return archive


def _add(ar, archive, objects, members):
    """Add one batch of objects, from the staging folder so that the members are bare names. `c`
    creates the archive if this is its first batch, and `s` writes the index the linker reads."""
    command = [ar, "crs" if not archive.exists() else "rs", str(archive), *members]
    completed = subprocess.run(command, cwd=objects)
    if completed.returncode != 0:
        sys.exit(f"{Path(ar).name} exited with {completed.returncode}")
