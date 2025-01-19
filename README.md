# Deprecation notice
Gtuber was a C library to fetch media info from websites.

This project was made to allow playback of media from various media services mainly in [Clapper](https://rafostar.github.io/clapper/) and possibly other [GStreamer](https://gitlab.freedesktop.org/gstreamer/gstreamer) based players with the benefit of acting like a media info extraction library. Unfortunately, due to all the changes occuring in services it tried to handle, this would need constant reverse enginering and maintenance on a daily basis.

As a replacement, [Clapper](https://rafostar.github.io/clapper/) now supports a [libpeas](https://gitlab.gnome.org/GNOME/libpeas) based plugin system which allows integrating other libraries to achive this functionality. This way, instead of trying to replicate this functionality here, other projects that focus on media extraction can continue their work, users can contribute fixes to them instead, one can write a plugin that uses such library and there is still absolutely zero web scrapping code in the player itself (a win-win solution).

Since [Clapper](https://rafostar.github.io/clapper/) nowadays provides an easy to use playback and GTK integration libraries (with `GObject Introspection` bindings), apps that are not video players in their entirety, but just need to play video as part of their functionality, can use those libraries with an optional addition of any extra plugins to enable playback from services they need.
