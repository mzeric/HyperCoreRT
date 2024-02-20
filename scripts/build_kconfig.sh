#!/usr/bin/env bash

package_file=kconfig-frontends-4.11.0.1.tar.bz2
patch_file=gperf3.1_kconfig_id_lookup.patch

base_url=http://klx-soc.bj.bcebos.com/bazel/download
kconfig_url=${base_url}/${package_file}
patch_url=${base_url}/${patch_file}

BUILD_DIR=build/kconfig-frontends

function download_build_kconfig {

	mkdir ${BUILD_DIR} 0p
	pushd ${BUILD_DIR}
	
	echo "downloading kconfig-fronted"
	wget ${kconfig_url}  -O ${package_file}
	wget ${patch_url} -O  ${patch_file}
	echo "download done"


	tar xf ${package_file}
	mv kconfig-frontends-4.11.0.1 kconfig  && rm -rf ${package_file}

	cd  kconfig

	patch -Np1 -i ../${patch_file}

	./configure  --disable-config-prefix && make && rm -rf ../${patch_file}

	popd

}

download_build_kconfig
