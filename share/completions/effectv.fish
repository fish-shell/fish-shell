# completions for effectv 0.3.9 or similiar

complete -c effectv -f -a "QuarkTV" -d "Dissolves moving objects"
complete -c effectv -f -a "FireTV" -d "Clips incoming objects and burns it"
complete -c effectv -f -a "BurningTV" -d "Burns incoming objects"
complete -c effectv -f -a "RadioacTV" -d "Brightens moving objects and blurs it"
complete -c effectv -f -a "StreakTV" -d "Makes 8 afterimages"
complete -c effectv -f -a "BaltanTV" -d "Makes afterimages longer"
complete -c effectv -f -a "1DTV" -d "Grabs a horizontal line from video every 1/30 sec"
complete -c effectv -f -a "DotTV" -d "Converts the picture into a set of dots"
complete -c effectv -f -a "MosaicTV" -d "Censors incoming objects"
complete -c effectv -f -a "PuzzleTV" -d "Scrambles the picture"
complete -c effectv -f -a "PredatorTV" -d "Makes incoming objects invisible visibly"
complete -c effectv -f -a "SpiralTV" -d "Spiraling effect"
complete -c effectv -f -a "SimuraTV" -d "Color distortion and image mirroring"
complete -c effectv -f -a "EdgeTV" -d "Detects edge and display it like good old computer way"
complete -c effectv -f -a "ShagadelicTV" -d "Go back to swinging '60s"
complete -c effectv -f -a "NoiseTV" -d "Makes incoming objects noisy"
complete -c effectv -f -a "AgingTV" -d "Film-aging effect"
complete -c effectv -f -a "LifeTV" -d "Conway's life game with video input interaction"
complete -c effectv -f -a "TransFormTV" -d "Performs positional translations on images"
complete -c effectv -f -a "SparkTV" -d "Spark effect"
complete -c effectv -f -a "warpTV" -d "Goo'ing effect"
complete -c effectv -f -a "HolographicTV" -d "Holographic vision seen in Star Wars"
complete -c effectv -f -a "cycleTV" -d "Color cycling effect"
complete -c effectv -f -a "RippleTV" -d "Ripple mark effect"
complete -c effectv -f -a "DiceTV" -d "A 'dicing' effect"
complete -c effectv -f -a "VertigoTV" -d "Alpha blending with zoomed and rotated images"
complete -c effectv -f -a "DeinterlaceTV" -d "Deinterlacing video images"
complete -c effectv -f -a "nervousTV" -d "Real-time frame shuffling effect"
complete -c effectv -f -a "RndmTV" -d "Gives you a noisy picture in color or B/W"
complete -c effectv -f -a "RevTV" -d "Waveform monitor effect"
complete -c effectv -f -a "RandomDotStereoTV" -d "Makes random dot stereo stream from video input"
complete -c effectv -f -a "LensTV" -d "Old school demo lens effect"
complete -c effectv -f -a "DiffTV" -d "Highlights interframe differences"
complete -c effectv -f -a "BrokenTV" -d "Simulates broken TV"
complete -c effectv -f -a "WarholTV" -d "Homage to Andy Warhol"
complete -c effectv -f -a "MatrixTV" -d "A Matrix like effect"
complete -c effectv -f -a "PUPTV" -d "Comes from \"Partial UPdate\", certain part of image is updated at a frame"
complete -c effectv -f -a "ChameleonTV" -d "Vanishing into the wall…"

complete -c effectv -o help -d "show usage information"
complete -c effectv -o device -r -d "[FILE] use device FILE for video4linux (default is /dev/video0)"
complete -c effectv -o channel -r -d "[CHANNEL] channel number of video source (default is 0)"

set __fish_effectv_norms "ntsc
pal
secam
pal-nc
pal-m
pal-n
ntsc-jp"
complete -c effectv -o norm -x -a '(echo $__fish_effectv_norms)' -d "[NORM] set video norm (default is ntsc)"

set __fish_effectv_freqs "argentina
australia
australia-optus
canada-cable
china-bcast
europe-east
europe-west
france
ireland
italy
japan-bcast
japan-cable
newzealand
southafrica
us-bcast
us-cable
us-cable-hrc"
complete -c effectv -o freqtab -x -a '(echo $__fish_effectv_freqs)' -d "[FREQUTAB] set frequency table"

complete -c effectv -o fullscreen -d "enable fullscreen mode"
complete -c effectv -o hardware -d "use direct video memory (if possible)"
complete -c effectv -o doublebuffer -d "enable double buffering mode (if possible)"
complete -c effectv -o fps -d "show frames/sec"
complete -c effectv -o size -x -a "WxH" -d "[WxH] set the size of capturing image to WxH pixels (default 320x240)"
complete -c effectv -o geometry -x -a "WxH" -d "[WxH] set the size of screen to WxH pixels"
complete -c effectv -o scale -x -d "[NUMBER] set the scaling. When the capturing size is set, the screen size is scaled by NUMBER"
complete -c effectv -o autoplay -x -d "[FRAMES] change effects automatically every FRAMES"

set __fish_effectv_palettes "grey
rgb24
rgb555
rgb565
yuv410p
yuv411p
yuv420p
yuv422
yuv422p"
complete -c effectv -o palette -x -a '(echo $__fish_effectv_palettes)' -d "[PALETTE] set the PALETTE of capturing device"

complete -c effectv -o vloopback -r -d "[FILE] use device FILE for output of vloopback device"
