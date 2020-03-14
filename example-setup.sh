# Should be sourced!

CHERI_ROOT=~/cheri/
cheri_sdk_dir="$(CHERI_ROOT)/output/sdk/bin"
PATH="${cheri_sdk_dir}:${PATH}"
CC=cheribsd128purecap-clang

export CHERI_ROOT
export CC
