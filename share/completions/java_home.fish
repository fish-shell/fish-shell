function __fish_complete_macos_java_version
    set -l xslt (mktemp)

    printf '%s\n' \
        '<?xml version="1.0" encoding="UTF-8"?>' \
        '<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">' \
        '  <xsl:output method="text" encoding="UTF-8"/>' \
        '  <xsl:template match="/">' \
        '    <xsl:for-each select="//dict">' \
        '      <xsl:variable name="v"   select="string(key[.=\'JVMVersion\']/following-sibling::*[1])"/>' \
        '      <xsl:variable name="a"   select="string(key[.=\'JVMArch\']/following-sibling::*[1])"/>' \
        '      <xsl:variable name="n"   select="string(key[.=\'JVMName\']/following-sibling::*[1])"/>' \
        '      <xsl:variable name="ven" select="string(key[.=\'JVMVendor\']/following-sibling::*[1])"/>' \
        '      <xsl:value-of select="concat($v,\'&#9;\',$a,\' \',$n,\' by \',$ven)"/>' \
        '      <xsl:text>&#10;</xsl:text>' \
        '    </xsl:for-each>' \
        '  </xsl:template>' \
        '</xsl:stylesheet>' >$xslt

    /usr/libexec/java_home -X | xsltproc $xslt -
end

function __fish_complete_macos_java_home_exec
    # seperate the buffer into two parts
    # where the first used to get the JAVA_HOME
    # and the second is the subcommand to complete
    set -l cmds (string replace -a -r ' *java_home *' ''  (commandline) )
    set -l cmds (string replace -r ' *--exec *' \n -- "$cmds")

    # parse the java_home argv to get $JAVA_HOME/bin
    argparse v/version= a/arch= -- "$cmds[1]"
    set -l get_java_home /usr/libexec/java_home
    if test -n "$_flag_v"
        set get_java_home "$get_java_home -v $_flag_v"
    end
    if test -n "$_flag_a"
        set get_java_home "$get_java_home -a $_flag_a"
    end
    set -l java_bin_dir (eval $get_java_home)"/bin"

    # if such $binary in $JAVA_HOME/bin
    # complete the subcommand
    # else
    # complete using $binary as prefix
    set -l binary (string match -r '^.*?(?= )' $cmds[2])
    if test -f "$java_bin_dir/$binary"
        complete -C $cmds[2]
    else
        command ls $java_bin_dir | string match -r ^"$binary.*"
    end
end

complete -ec java_home
complete -xc java_home -n "__fish_not_contain_opt -s h exec " -l exec
complete -xc java_home -n "__fish_contains_opt exec " -a "(__fish_complete_macos_java_home_exec)"
complete -xc java_home -n "__fish_not_contain_opt -s h exec " -s v -l version -a '(__fish_complete_macos_java_version)' -d 'Filter versions (as if JAVA_VERSION had been set in the environment).'
complete -xc java_home -n "__fish_not_contain_opt -s h exec " -s a -l arch -a "arm64 x86_64" -d 'Filter architecture (as if JAVA_ARCH had been set in the environment).'
complete -xc java_home -n "__fish_not_contain_opt -s h exec " -s h -l help -d 'Usage information.'
complete -fc java_home -n "__fish_not_contain_opt -s h exec " -s F -l failfast -d 'Fail when filters return no JVMs, do not continue with default.'
complete -fc java_home -n "__fish_not_contain_opt -s h exec " -s X -l xml -d 'Print full JVM list and additional data as XML plist.'
complete -fc java_home -n "__fish_not_contain_opt -s h exec " -s V -l verbose -d 'Print full JVM list with architectures.'
