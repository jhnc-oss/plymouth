#! /bin/bash
set -e
renice --priority 10 "${$}" >/dev/null

here="$(realpath "$(dirname "${0}")")"
build="${here}/_BUILD"
root="${here}/_ROOT"

# For debugging (default "true"):
recompile=true


mainFunction () {
	build
	makeRoot
}


build () {
	local lock="${here}/_BUILD.lock"

	if "${recompile}" || [[ ! -f "${lock}" ]] || [[ ! -d "${build}" ]]; then
		so rm --force "${lock}"
		makeBuildDir
		cd "${build}"

		mesonSetup . _MESON
		so meson compile -C _MESON --jobs "$(nproc)"

		touch "${lock}"
	fi
}


makeBuildDir () {
	local file
	local files; files="$(
		find . -maxdepth 1 \
		-not -name . \
		-and -not -name "build.sh" \
		-and -not -name "_BUILD" \
		-and -not -name "_ROOT"
	)"

	so rm --recursive --force "${build}"
	so mkdir --parents "${build}/build-tools"

	for file in "${files[@]}"; do
		so cp --recursive "${file}" "${build}/"
	done
}


makeRoot () {
	cd "${build}/_MESON"
	so rm --recursive --force "${root}"
	so meson install --destdir "${root}"
	find "${root}" -empty -type d -delete
}


mesonSetup () {
	so meson setup \
		--prefix        /usr \
		--libexecdir    lib \
		--sbindir       bin \
		--buildtype     plain \
		--auto-features enabled \
		--wrap-mode     nodownload \
		-D              b_pie=true \
		-D              python.bytecompile=1 \
		"${@}"
}


so () {
	local commands="${*}"
	local maxLines=20
	local error=""

	#shellcheck disable=SC2068
	if ! error="$(${commands[@]} 2>&1)"; then
		if [[ -z "${error}" ]] ; then
			error="${commands[*]}: failed}"
		else
			if [[ "$(echo "${error}" | wc --lines)" -gt "${maxLines}" ]]; then
				error="$(echo "${error}" | tail -n "${maxLines}")"
				error="$(printf "...\n%s", "${error}")"
			fi

			echo "${commands[*]}:" >&2
			echo "${error}" >&2
		fi

		exit 1
	fi
}


mainFunction
