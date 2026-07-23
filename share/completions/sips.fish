# sips - scriptable image processing system (man 1 sips)
#
# sips uses flags (not subcommands). Many flags are mutually usable in sequence.
# We offer flag completions globally; property names and format values are
# enumerated as arguments to -g/--getProperty, -s/--setProperty, -d/--deleteProperty,
# and -s format / --setProperty format.

# ── helper: true when the previous token on the commandline is one of the given values ──
function __fish_sips_prev_is
    set -l tokens (commandline -opc)
    set -l n (count $tokens)
    if test $n -lt 2
        return 1
    end
    contains -- $tokens[$n] $argv
end

# ── property name enumerators ─────────────────────────────────────────────────

# Image property keys (readable/writable unless noted read-only)
function __fish_sips_image_props
    printf '%s\t%s\n' \
        dpiHeight 'float — vertical DPI' \
        dpiWidth 'float — horizontal DPI' \
        pixelHeight 'integer (read-only)' \
        pixelWidth 'integer (read-only)' \
        typeIdentifier 'string (read-only)' \
        format 'string — output format' \
        formatOptions 'string — compression options' \
        space 'string (read-only)' \
        samplesPerPixel 'integer (read-only)' \
        bitsPerSample 'integer (read-only)' \
        creation 'string (read-only)' \
        make string \
        model string \
        software 'string (read-only)' \
        description string \
        copyright string \
        artist string \
        profile 'binary data' \
        hasAlpha 'boolean (read-only)' \
        all 'binary data — all properties' \
        allxml 'binary data — all properties as XML'
end

# Profile property keys
function __fish_sips_profile_props
    printf '%s\t%s\n' \
        description 'utf8 string' \
        size 'integer (read-only)' \
        cmm string \
        version string \
        class 'string (read-only)' \
        space 'string (read-only)' \
        pcs 'string (read-only)' \
        creation string \
        platform string \
        quality 'string — normal | draft | best' \
        deviceManufacturer string \
        deviceModel integer \
        deviceAttributes0 integer \
        deviceAttributes1 integer \
        renderingIntent 'string — perceptual | relative | saturation | absolute' \
        creator string \
        copyright string \
        md5 'string (read-only)' \
        all 'binary data — all properties' \
        allxml 'binary data — all properties as XML'
end

# All property keys (union; used for -g/--getProperty which works on both image and profile)
function __fish_sips_all_props
    __fish_sips_image_props
    __fish_sips_profile_props
end

# Format values for `sips -s format <value>`
function __fish_sips_formats
    printf '%s\t%s\n' \
        jpeg JPEG \
        tiff TIFF \
        png PNG \
        gif GIF \
        jp2 'JPEG 2000' \
        pict PICT \
        bmp BMP \
        qtif 'QuickTime Image' \
        psd 'Photoshop PSD' \
        sgi SGI \
        tga 'Targa TGA'
end

# formatOptions values
function __fish_sips_format_options
    printf '%s\t%s\n' \
        default 'Default compression' \
        low 'Low quality' \
        normal 'Normal quality' \
        high 'High quality' \
        best 'Best quality' \
        lzw 'LZW compression (TIFF)' \
        packbits 'PackBits compression (TIFF)'
end

# Rendering intent values (for -M/--matchToWithIntent)
function __fish_sips_intents
    printf '%s\t%s\n' \
        perceptual 'Perceptual rendering intent' \
        relative 'Relative colorimetric' \
        saturation 'Saturation rendering intent' \
        absolute 'Absolute colorimetric'
end

# Flip values (for -f/--flip)
function __fish_sips_flip_axes
    printf '%s\t%s\n' \
        horizontal 'Flip horizontally' \
        vertical 'Flip vertically'
end

# true when `-s format` (or `--setProperty format`) was typed and we want the format value
function __fish_sips_wants_format_value
    set -l tokens (commandline -opc)
    set -l n (count $tokens)
    if test $n -lt 3
        return 1
    end
    # two tokens back must be -s/--setProperty, one back must be "format"
    if contains -- $tokens[(math $n - 1)] -s --setProperty
        and test "$tokens[$n]" = format
        return 0
    end
    return 1
end

# true when `-s formatOptions` was typed and we want the options value
function __fish_sips_wants_formatopts_value
    set -l tokens (commandline -opc)
    set -l n (count $tokens)
    if test $n -lt 3
        return 1
    end
    if contains -- $tokens[(math $n - 1)] -s --setProperty
        and test "$tokens[$n]" = formatOptions
        return 0
    end
    return 1
end

# true when the previous token expects a profile file argument
function __fish_sips_wants_profile
    __fish_sips_prev_is -e --embedProfile -E --embedProfileIfNone -m --matchTo -M --matchToWithIntent
end

# true when prev token is -M/--matchToWithIntent and a profile has already been supplied
# (i.e. we are at the intent argument position)
function __fish_sips_wants_intent
    set -l tokens (commandline -opc)
    set -l n (count $tokens)
    # walk backwards: if we find -M/--matchToWithIntent two positions back, the
    # immediately previous token is the profile and we now want the intent.
    if test $n -ge 3
        if contains -- $tokens[(math $n - 1)] -M --matchToWithIntent
            return 0
        end
    end
    return 1
end

# true when prev token is -f/--flip
function __fish_sips_wants_flip
    __fish_sips_prev_is -f --flip
end

# true when prev token expects a JavaScript file
function __fish_sips_wants_js
    __fish_sips_prev_is -j --js
end

# true when prev token is -X/--extractTag or --deleteTag or --copyTag or --loadTag
function __fish_sips_wants_tag
    __fish_sips_prev_is -X --extractTag --deleteTag --copyTag --loadTag
end

# ── image query functions ─────────────────────────────────────────────────────
complete -c sips -s g -l getProperty -d 'Output the property value for key'
complete -c sips -n '__fish_sips_prev_is -g --getProperty' -x -a '(__fish_sips_all_props)'

complete -c sips -s x -l extractProfile -d 'Get embedded profile from image and write to file' -r

complete -c sips -s 1 -l oneLine -d 'Output each file on a single line (pipe-delimited for profiles, tab for images)'

# ── profile query functions ───────────────────────────────────────────────────
# -g/--getProperty and -1/--oneLine also apply to profiles (already listed above)
complete -c sips -s X -l extractTag -d 'Write a profile tag element to tagFile'
complete -c sips -l verify -d 'Verify any profile problems and log to stdout'

# ── profile modification functions ───────────────────────────────────────────
complete -c sips -l deleteTag -d 'Remove a tag element from a profile'
complete -c sips -l copyTag -d 'Copy srcTag element of a profile to dstTag'
complete -c sips -l loadTag -d 'Set a tag element of a profile to the contents of tagFile'
complete -c sips -l repair -d 'Repair any profile problems and log to stdout'

# ── image modification functions ─────────────────────────────────────────────
complete -c sips -s s -l setProperty -d 'Set a property value for key to value'
complete -c sips -n '__fish_sips_prev_is -s --setProperty' -x -a '(__fish_sips_all_props)'
complete -c sips -n __fish_sips_wants_format_value -x -a '(__fish_sips_formats)'
complete -c sips -n __fish_sips_wants_formatopts_value -x -a '(__fish_sips_format_options)'

complete -c sips -s d -l deleteProperty -d 'Remove a property value for key'
complete -c sips -n '__fish_sips_prev_is -d --deleteProperty' -x -a '(__fish_sips_all_props)'

complete -c sips -s e -l embedProfile -d 'Embed profile in image' -r
complete -c sips -s E -l embedProfileIfNone -d 'Embed profile in image only if none present' -r
complete -c sips -s m -l matchTo -d 'Color match image to profile' -r
complete -c sips -s M -l matchToWithIntent -d 'Color match image to profile with rendering intent' -r
complete -c sips -n __fish_sips_wants_intent -x -a '(__fish_sips_intents)'

complete -c sips -l deleteColorManagementProperties -d 'Delete color management properties in TIFF, PNG, and EXIF'

complete -c sips -s r -l rotate -d 'Rotate image clockwise by degrees' -x -a '90 180 270'

complete -c sips -s f -l flip -d 'Flip image horizontal or vertical'
complete -c sips -n __fish_sips_wants_flip -x -a '(__fish_sips_flip_axes)'

complete -c sips -s c -l cropToHeightWidth -d 'Crop image to fit specified height and width (pixels)'
complete -c sips -l cropOffset -d 'Crop offset from top left corner (offsetY offsetH)'

complete -c sips -s p -l padToHeightWidth -d 'Pad image with pixels to fit specified height and width'
complete -c sips -l padColor -d 'Hex color used when padding (e.g. FFFFFF=white, FF0000=red, default=000000)'

complete -c sips -s z -l resampleHeightWidth -d 'Resample image to specified size (aspect ratio may change)'
complete -c sips -l resampleWidth -d 'Resample image to specified width (maintains aspect ratio)'
complete -c sips -l resampleHeight -d 'Resample image to specified height (maintains aspect ratio)'
complete -c sips -s Z -l resampleHeightWidthMax -d 'Resample so height and width are no greater than specified size'

complete -c sips -s i -l addIcon -d 'Add a Finder icon to image file'
complete -c sips -l optimizeColorForSharing -d 'Optimize color for sharing'

# ── output destination ────────────────────────────────────────────────────────
complete -c sips -s o -l out -d 'Write output to file or directory' -r

# ── JavaScript execution ──────────────────────────────────────────────────────
complete -c sips -s j -l js -d 'Execute JavaScript file' -r
# force .js extension hint for the file argument after -j/--js
complete -c sips -n __fish_sips_wants_js -x -a '(
    __fish_complete_suffix .js
)'

# ── other / informational functions ──────────────────────────────────────────
complete -c sips -l debug -d 'Enable debugging output'
complete -c sips -s h -l help -d 'Show help'
complete -c sips -s H -l helpProperties -d 'Show help for properties'
complete -c sips -l man -d 'Generate man pages'
complete -c sips -s v -l version -d 'Show the version'
complete -c sips -l formats -d 'Show the read/write formats'
