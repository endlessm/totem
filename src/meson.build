src_inc = include_directories('.')
plugins_inc = include_directories('plugins')

subdir('gst')
subdir('backend')
subdir('properties')

enum_headers = files(
  'backend/bacon-time-label.h',
  'backend/bacon-video-controls-actor.h',
  'backend/bacon-video-spinner-actor.h',
  'backend/bacon-video-widget-gst-missing-plugins.h',
  'backend/bacon-video-widget.h',
  'backend/clock.h',
  'backend/totem-aspect-frame.h',
  'icon-helpers.h',
  'totem-grilo.h',
  'totem-interface.h',
  'totem-main-toolbar.h',
  'totem-menu.h',
  'totem-open-location.h',
  'totem-options.h',
  'totem-playlist.h',
  'totem-preferences.h',
  'totem-private.h',
  'totem-profile.h',
  'totem-properties-view.h',
  'totem-resources.h',
  'totem-search-entry.h',
  'totem-selection-toolbar.h',
  'totem-session.h',
  'totem-subtitle-encoding.h',
  'totem-time-label.h',
  'totem-uri.h',
  'totem.h'
)

gnome.mkenums(
  'org.gnome.totem.enums.xml',
  sources: enum_headers,
  comments: '<!-- @comment@ -->',
  fhead: '<schemalist>',
  vhead: '  <@type@ id="org.gnome.totem.@EnumName@">',
  vprod: '    <value nick="@valuenick@" value="@valuenum@"/>',
  vtail: '  </@type@>',
  ftail: '</schemalist>',
  install_header: true,
  install_dir: totem_schemadir
)

totem_common_incs = [
  top_inc,
  plugins_inc
]

totem_common_deps = libbacon_video_widget_deps + python_deps + [
  glib_dep,
  gio_dep,
  gtk_dep,
  gdk_x11_dep,
  dependency('gmodule-2.0'),
  totem_plparser_dep,
  peas_dep,
  peas_gtk_dep,
  dependency('grilo-0.3', version: grilo_req_version),
  dependency('grilo-pls-0.3', version: grilo_req_version),
  dependency('gnome-desktop-3.0')
]

totem_common_cflags = common_flags + warn_flags + [
  '-DDBUS_API_SUBJECT_TO_CHANGE',
  '-DGNOMELOCALEDIR="@0@"'.format(totem_localedir),
  '-DDATADIR="@0@"'.format(totem_datadir),
  '-DLIBEXECDIR="@0@"'.format(totem_libexecdir),
  '-DTOTEM_PLUGIN_DIR="@0@"'.format(totem_pluginsdir),
  '-DBINDIR="@0@"'.format(totem_bindir),
  '-DGTKBUILDERDIR=""'
]

headers = files(
  'plugins/totem-plugin.h',
  'plugins/totem-dirs.h',
  'totem-interface.h',
  'totem.h'
)

install_headers(
  headers,
  subdir: join_paths(totem_includedir, totem_api_path)
)

libtotem_player_sources = files(
  'totem-interface.c',
  'totem-time-label.c'
)

libtotem_player_deps = totem_common_deps + [
  libtotem_time_helpers_dep
]

libtotem_player = static_library(
  'totem_player',
  sources: libtotem_player_sources,
  include_directories: top_inc,
  dependencies: libtotem_player_deps,
  c_args: totem_common_cflags + [
    '-DG_LOG_DOMAIN="Totem"'
  ]
)

libtotem_player_dep = declare_dependency(
  link_with: libtotem_player,
  include_directories: src_inc,
  dependencies: libtotem_player_deps
)

libtotem_sources = files(
  'plugins/totem-dirs.c',
  'plugins/totem-plugins-engine.c',
  'icon-helpers.c',
  'totem-grilo.c',
  'totem-main-toolbar.c',
  'totem-menu.c',
  'totem-object.c',
  'totem-open-location.c',
  'totem-options.c',
  'totem-playlist.c',
  'totem-preferences.c',
  'totem-search-entry.c',
  'totem-selection-toolbar.c',
  'totem-session.c',
  'totem-subtitle-encoding.c',
  'totem-uri.c'
)

libtotem_sources += gnome.compile_resources(
  'griloresources',
  'grilo.gresource.xml',
  export: true,
  source_dir: '.',
  c_name: '_totem'
)

libtotem_deps = [
  libtotem_player_dep,
  libbacon_video_widget_dep,
  libgd_dep
]

libtotem = shared_library(
  'totem',
  sources: libtotem_sources + gen_sources,
  version: libversion,
  soversion: soversion,
  include_directories: totem_common_incs,
  dependencies: libtotem_deps,
  c_args: totem_common_cflags + [
    '-DG_LOG_DOMAIN="Totem"'
  ],
  install: true,
  install_dir: totem_libdir
)

libtotem_dep = declare_dependency(
  link_with: libtotem,
  include_directories: src_inc,
  dependencies: libtotem_deps
)

pkg.generate(
  libraries: libtotem,
  version: totem_version,
  name: 'Totem',
  description: 'Totem Movie Player plugin API',
  filebase: meson.project_name(),
  subdirs: totem_api_path,
  requires: [
    'gio-2.0 ' + glib_req_version,
    'gtk+-3.0 ' + gtk_req_version,
    'totem-plparser ' + totem_plparser_req_version
  ],
  variables: [
    'exec_prefix=' + totem_libexecdir,
    'pluginsdir=' + totem_pluginsdir
  ],
  install_dir: join_paths(totem_libdir, 'pkgconfig')
)

totem = executable(
  meson.project_name(),
  'totem.c',
  include_directories: totem_common_incs,
  dependencies: libtotem_dep,
  c_args: totem_common_cflags + [
    '-DG_LOG_DOMAIN="Totem"'
  ],
  install: true,
  install_dir: totem_bindir
)

totem_video_thumbnailer_sources = files(
  'totem-resources.c',
  'totem-video-thumbnailer.c'
)

totem_video_thumbnailer_deps = [
  dependency('gdk-3.0'),
  totem_plparser_dep,
  gst_tag_dep,
  gst_video_dep,
  m_dep,
  libtotem_gst_helpers_dep,
  libtotem_gst_pixbuf_helpers_dep,
  libtotem_time_helpers_dep
]

executable(
  'totem-video-thumbnailer',
  totem_video_thumbnailer_sources,
  include_directories: top_inc,
  dependencies: totem_video_thumbnailer_deps,
  c_args: totem_common_cflags + [
    '-DG_LOG_DOMAIN="TotemVideoThumbnailer"'
  ],
  install: true,
  install_dir: totem_bindir
)

totem_gallery_thumbnailer_sources = files(
  'totem-gallery-thumbnailer.c'
)

totem_gallery_thumbnailer_deps = [
  dependency('gdk-3.0'),
  totem_plparser_dep,
  gst_tag_dep,
  gst_video_dep,
  m_dep,
  libtotem_gst_helpers_dep,
  libtotem_gst_pixbuf_helpers_dep,
  libtotem_time_helpers_dep
]

executable(
  'totem-gallery-thumbnailer',
  totem_gallery_thumbnailer_sources,
  include_directories: top_inc,
  dependencies: totem_gallery_thumbnailer_deps,
  c_args: totem_common_cflags + [
    '-DG_LOG_DOMAIN="TotemGalleryThumbnailer"'
  ],
  install: true,
  install_dir: totem_libexecdir
)

if have_nautilus
  libtotem_properties_page_sources = files(
    'totem-properties-main.c',
    'totem-properties-view.c'
  )

  libtotem_properties_page_deps = missing_plugins_deps + [
    gtk_dep,
    dependency('gthread-2.0'),
    libnautilus_ext_dep,
    gst_tag_dep,
    libbacon_video_widget_properties_dep
  ]

  libtotem_properties_page = shared_module(
    'totem-properties-page',
    sources: libtotem_properties_page_sources + gen_sources,
    include_directories: top_inc,
    dependencies: libtotem_properties_page_deps,
    c_args: totem_common_cflags + [
      '-DG_LOG_DOMAIN="TotemPropertiesPage"'
    ],
    install: true,
    install_dir: nautilusdir
  )

  test_properties_page_sources = files(
    'totem-properties-main.c',
    'totem-properties-view.c',
    'test-properties-page.c'
  )

  executable(
    'test-properties-page',
    test_properties_page_sources + gen_sources,
    include_directories: top_inc,
    dependencies: libtotem_properties_page_deps + [
      libtotem_player_dep,
      libbacon_video_widget_dep
    ],
    c_args: totem_common_cflags
  )
endif

test_icons_sources = files(
  'icon-helpers.c',
  'test-icons.c'
)

executable(
  'test-icons',
  test_icons_sources,
  dependencies: [
    libtotem_player_dep,
    libbacon_video_widget_dep
  ],
  c_args: totem_common_cflags
)

if have_gir
  gir_sources = libtotem_sources + libtotem_player_sources + headers

  gir_incs = [
    'Gio-2.0',
    'Gtk-3.0',
    'TotemPlParser-1.0'
  ]

  gir_extra_args = [
    '--c-include=totem.h',
    '--pkg-export=totem'
  ]

  gir_dir = join_paths(totem_datadir, 'gir-' + totem_api_version)
  typelib_dir = join_paths(totem_libdir, 'girepository-' + totem_api_version)

  libtotem_gir = gnome.generate_gir(
    libtotem,
    sources: gir_sources,
    nsversion: totem_api_version,
    namespace: totem_gir_ns,
    symbol_prefix: totem_gir_ns.to_lower(),
    identifier_prefix: totem_gir_ns,
    includes: gir_incs,
    include_directories: src_inc,
    install: true,
    install_dir_gir: gir_dir,
    install_dir_typelib: typelib_dir,
    extra_args: gir_extra_args
  )

  if have_vala
    libtotem_vapi = gnome.generate_vapi(
      'totem',
      sources: libtotem_gir[0],
      packages: ['gio-2.0', 'gtk+-3.0', 'TotemPlParser-1.0'],
      install: false,
    )
  endif
endif

subdir('plugins')