project('naoland', ['cpp'], default_options : [
  'cpp_std=c++20',
  'warning_level=3'
])

cpp_compiler = meson.get_compiler('cpp')
if cpp_compiler.has_argument('-Wno-gnu-zero-variadic-macro-arguments')
  add_project_arguments('-Wno-gnu-zero-variadic-macro-arguments', language : 'cpp')
endif

wlroots_dep = dependency('wlroots',
                         version: ['>= 0.17', '< 0.18.0'],
                         fallback: ['wlroots'],
                         default_options: ['examples=false'])

foreach feature : ['drm_backend', 'libinput_backend', 'session', 'xwayland']
    var_feature = 'have_' + feature
    if wlroots_dep.get_variable(pkgconfig: var_feature, internal : var_feature, default_value: 'false') != 'true'
        error('Wlroots was not built with ' + feature + ' support. Check for missing dependencies.')
    endif
endforeach

subdir('comp')
