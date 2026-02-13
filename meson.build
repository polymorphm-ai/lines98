project(
  'lines98',
  'c',
  version: '0.1.0',
  meson_version: '>=1.2.0',
  default_options: [
    'c_std=c11'
  ]
)

add_project_arguments('-D_POSIX_C_SOURCE=200809L', language: 'c')

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)
strict_c_args = cc.get_supported_arguments([
  '-Wall',
  '-Wextra',
  '-Wpedantic',
  '-Werror',
])

inc = include_directories('src')

core_sources = [
  'src/game.c',
  'src/rng.c',
  'src/turn_controller.c',
  'src/turn_anim.c',
]

if get_option('build_game')
  sdl2_dep = dependency('sdl2', required: true, fallback: ['sdl2', 'SDL2_dep'])

  executable(
    'lines98',
    ['src/main.c', 'src/audio_fx.c', 'src/fx_particles.c', 'src/render_ui.c'] + core_sources,
    include_directories: inc,
    dependencies: [sdl2_dep, m_dep],
    c_args: strict_c_args,
    install: true,
  )

  audio_fx_exe = executable(
    'lines98_audio_fx_tests',
    ['tests/test_audio_fx.c', 'src/audio_fx.c'],
    include_directories: inc,
    dependencies: [sdl2_dep, m_dep],
    c_args: strict_c_args,
  )
endif

test_exe = executable(
  'lines98_tests',
  ['tests/test_game.c'] + core_sources,
  include_directories: inc,
  c_args: strict_c_args,
)

stress_exe = executable(
  'lines98_stress_tests',
  ['tests/test_stress.c'] + core_sources,
  include_directories: inc,
  c_args: strict_c_args,
)

turn_anim_exe = executable(
  'lines98_turn_anim_tests',
  ['tests/test_turn_anim.c'] + core_sources,
  include_directories: inc,
  c_args: strict_c_args,
)

turn_controller_exe = executable(
  'lines98_turn_controller_tests',
  ['tests/test_turn_controller.c'] + core_sources,
  include_directories: inc,
  c_args: strict_c_args,
)

test(
  'core-tests',
  test_exe,
  env: [
    'ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1',
  ],
)

test(
  'core-stress-tests',
  stress_exe,
  env: [
    'ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1',
  ],
)

test(
  'turn-anim-tests',
  turn_anim_exe,
  env: [
    'ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1',
  ],
)

test(
  'turn-controller-tests',
  turn_controller_exe,
  env: [
    'ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1',
  ],
)

if get_option('enable_lsan')
  test(
    'core-tests-lsan',
    test_exe,
    env: [
      'ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1',
      'LSAN_OPTIONS=detect_leaks=1',
    ],
  )

  test(
    'core-stress-tests-lsan',
    stress_exe,
    env: [
      'ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1',
      'LSAN_OPTIONS=detect_leaks=1',
    ],
  )
endif

if get_option('build_game')
  test(
    'audio-fx-safety-tests',
    audio_fx_exe,
    env: [
      'ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1',
    ],
  )
endif

valgrind = find_program('valgrind', required: false)
if valgrind.found()
  test(
    'core-tests-valgrind',
    valgrind,
    args: [
      '--quiet',
      '--leak-check=full',
      '--show-leak-kinds=all',
      '--errors-for-leak-kinds=all',
      '--error-exitcode=1',
      test_exe,
    ],
  )

  test(
    'core-stress-tests-valgrind',
    valgrind,
    args: [
      '--quiet',
      '--leak-check=full',
      '--show-leak-kinds=all',
      '--errors-for-leak-kinds=all',
      '--error-exitcode=1',
      stress_exe,
    ],
  )
endif
