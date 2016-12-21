# toaru-playmp3

This is an MP3 player for ToaruOS using [KeyJ](http://keyj.emphy.de/minimp3/)'s `minimp3` library. That library is released under the LGPL 2.1. This application is released under the ISC license.

On ToaruOS, this will only play 48KHz `s16le` MP3s as the underlying library does no sample rate conversion and our audio interface only supports 48KHz `s16le` audio. You can convert MP3s to this format with `sox`.
