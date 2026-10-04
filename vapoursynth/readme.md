## ReduceFlicker for VapourSynth
	This is a port of Avisynth's ReduceFlicker which written by Rainer Wittmann.
	This plugin has only ReduceFlicker(). ReduceFluctuation() and LockClense() are not implemented.

### Requirements:
	- VapourSynth R55 or later (API 4)

### Syntax:
	rdfl.ReduceFlicker(vnode clip[, int strength, int aggressive, int[] planes, int opt])

#### clip:
	All formats except half precision are supported: 8-16 bit integer and 32 bit float,
	Gray, YUV and RGB with any subsampling. The clip must have a constant format.

#### strength:
	Specify the strength of ReduceFlicker. Higher values mean more aggressive operation.

	1 - makes use of 4(current + 2*previous + 1*next) frames .
	2(default) - makes use of 5(current + 2*previous + 2*next) frames.
	3 - makes use of 7(current + 3*previous + 3*next) frames.

	Frames beyond the start or the end of the clip are replaced by the first or
	last frame.

#### aggressive:
	If set this to 1, then a significantly more aggressive variant of the algorithm is selected.
	Default value is 0.

#### planes:
	Whether planes will be processed or not. If set this to 0, the plane will be copied from source clip.
	Default values are [1, 1, 1](all planes will be processed).

	examples
		planes=[0, 1, 1] -> chroma (or G/B planes) will be processed.
		planes=[0, 0, 0] -> do nothing (all planes will be copied from source).

#### opt:
	Controls which cpu optimizations are used. All routines produce identical output.

	0 - Use C++ routine.
	1 - Use SSE2/SSE routine. If cpu does not have SSE2, fallback to 0.
	2 - Use SSE4.1/SSE2/SSE routine. If cpu does not have SSE4.1, fallback to 1.
	3(default) - Use AVX2/AVX routine. If cpu does not have AVX2, fallback to 2.

	On arm64 (e.g. macOS on Apple silicon) there is one NEON routine: 0 uses the C++ routine,
	every other value NEON. On other non x86 machines only the C++ routine exists and opt is ignored.

### Installation:
	Prebuilt wheels for Windows x64, Linux x86_64 and macOS arm64 are attached to each
	GitHub release (https://github.com/Selur/ReduceFlicker/releases):

		pip install vapoursynth_reduceflicker-*.whl

	The plugin is installed into the plugin directory of the VapourSynth Python
	package and autoloaded as core.rdfl.

### Compilation:
	Meson and Ninja are required. The VapourSynth API 4 headers are bundled,
	a system installation of VapourSynth is optional.

		cd vapoursynth
		meson setup build
		ninja -C build

	On macOS the plugin is built as libReduceFlicker.dylib, which is the only
	extension VapourSynth autoloads there.

### Testing:
	test/test_reduceflicker.py compares every strength / aggressive / opt combination
	against a numpy transcription of the algorithm at 8-16 bit and float. It needs
	the vapoursynth Python module and numpy:

		python3 test/test_reduceflicker.py build/libReduceFlicker.so

### Lisence:
	LGPLv2.1 or later.

### Source code:
	https://github.com/chikuzen/ReduceFlicker/vapoursynth/
