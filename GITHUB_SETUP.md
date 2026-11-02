# GitHub repository setup

Suggested repository name: **ultimate-community-edition**.

Suggested description: **Community firmware for the original Ultimate 64 and Elite, based on Ultimate firmware 3.15a.**

README.md is the project landing page. CHANGELOG.md describes **UCE 1.0.0**, the initial UCE release. CREDITS.md, AUTHORS.txt, PROVENANCE.md and LICENSE.txt retain upstream and contributor attribution.

## Create the initial UCE branch

The source archive includes a cumulative `maintainer/uce-initial.patch` for the pinned upstream baseline. This route preserves upstream Git history while introducing UCE as one initial community commit:

```bash
git clone https://github.com/GideonZ/1541ultimate.git ultimate-community-edition
cd ultimate-community-edition
git checkout -b uce-original-u64 dddd29b25d64c79d3a8cab95d8b86e1f8cafb816
git submodule update --init --recursive
git apply --check /root/uce-1.0.0/maintainer/uce-initial.patch &&
git apply /root/uce-1.0.0/maintainer/uce-initial.patch
git add --pathspec-from-file=/root/uce-1.0.0/maintainer/PATCH_FILES.txt
git diff --cached --stat
git diff --cached
git commit -m "Initial Ultimate Community Edition release"
```

Set your Git author identity before committing if needed. Build from the committed tree using README.md so generated build information identifies the source. The archive's source/ folder is a convenience snapshot without Git metadata.

## Publish the repository

Create an empty GitHub repository under your account, without generating another README or license. Use your actual repository URL:

```bash
git remote rename origin upstream
git remote add origin YOUR_UCE_REPOSITORY_URL
git push -u origin uce-original-u64
```

Set `uce-original-u64` as the default branch. Publish only intended UCE branches/tags, not a mirror of every upstream ref. The archive's maintainer/ directory contains local setup aids and need not be copied into the public source tree.

## First firmware release

Complete the target build and hardware acceptance before marking the firmware stable. Then tag the tested commit as `v1.0.0` and attach `update.u64`, its SHA-256 and release notes. State supported boards, source commit, toolchain and bridge versions, and operating limits. Keep corresponding source and license notices available.

The delivered package contains source and does not publish to GitHub or supply a precompiled UCE updater.
