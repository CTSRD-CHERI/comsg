# Should be sourced!

CHERI_ROOT=~/cheri/
cheri_sdk_dir="$(CHERI_ROOT)/output/sdk"
PATH="${cheri_sdk_dir}/bin:${PATH}"
CC=clang-11
cc=$CC

export CHERI_ROOT
export CC

