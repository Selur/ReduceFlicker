project('ReduceFlicker', 'cpp',
  default_options : ['buildtype=release', 'b_ndebug=if-release', 'cpp_std=c++14'],
  meson_version : '>=0.51.0',
  version : '1.0'
)

cxx = meson.get_compiler('cpp')
gcc_syntax = cxx.get_argument_syntax() == 'gcc'

conf = configuration_data()
conf.set_quoted('REDUCEFLICKER_VERSION', meson.project_version())
configure_file(output : 'config.h', configuration : conf)

# Prefer system VapourSynth headers, fall back to the vendored API 4 headers.
system_vs_dep = dependency('vapoursynth', version : '>=55', required : false)
if system_vs_dep.found()
  vapoursynth_dep = system_vs_dep.partial_dependency(compile_args : true, includes : true)
else
  vapoursynth_dep = declare_dependency(include_directories : include_directories('include/vapoursynth'))
endif

sources = [
  'src/ReduceFlicker.cpp',
  'src/plugin.cpp',
  'src/proc_c.cpp',
]

# The SIMD routines are x86 only. Every instruction set is compiled in its own
# unit with the matching flags; the one used is picked at runtime (see arch.h).
simd_libs = []
if host_machine.cpu_family() in ['x86', 'x86_64']
  sources += 'src/cpu_check.cpp'
  if gcc_syntax
    sse2_args = ['-msse2']
    sse41_args = ['-msse4.1']
    avx2_args = ['-mavx2']
  else
    sse2_args = []
    sse41_args = []
    avx2_args = ['/arch:AVX2']
  endif
  foreach isa : [['sse2', sse2_args], ['sse41', sse41_args], ['avx2', avx2_args]]
    simd_libs += static_library('reduceflicker_' + isa[0], 'src/proc_' + isa[0] + '.cpp',
      cpp_args : isa[1],
      dependencies : vapoursynth_dep,
      pic : true,
      gnu_symbol_visibility : 'hidden'
    )
  endforeach
endif

# VapourSynth only autoloads *.dylib on macOS, while Meson defaults to *.so for modules.
suffix = host_machine.system() == 'darwin' ? 'dylib' : []

if get_option('python_package')
  # Same layout as the VapourSynth PyPI package autoloads from: site-packages/vapoursynth/plugins
  py = import('python').find_installation(pure : false)
  install_dir = join_paths(py.get_install_dir(), 'vapoursynth', 'plugins')
else
  libdir = join_paths(get_option('prefix'), get_option('libdir'))
  if system_vs_dep.found()
    libdir = system_vs_dep.get_variable(pkgconfig : 'libdir', default_value : libdir)
  endif
  install_dir = join_paths(libdir, 'vapoursynth')
endif

shared_module('ReduceFlicker', sources,
  dependencies : vapoursynth_dep,
  link_with : simd_libs,
  install : true,
  install_dir : install_dir,
  name_suffix : suffix,
  gnu_symbol_visibility : 'hidden'
)
