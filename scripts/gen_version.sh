#!/usr/bin/env bash

GIT_COMMIT_HASH=$(git describe --always --dirty)

sed -i "s/^#define GIT_COMMIT_HASH.*/#define GIT_COMMIT_HASH \"${GIT_COMMIT_HASH}\"/" include/version.h
