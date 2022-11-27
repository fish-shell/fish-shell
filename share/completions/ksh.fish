set -l options allexport bgnice braceexpand emacs errexit globstar gmacs histexpand ignoreeof keyword letoctal markdirs monitor multiline noclobber noexec noglob nolog notify nounset pipefail privileged showme trackall verbose vi viraw xtrace
complete -c ksh -a "+{a,b,c,e,f,h,i,k,n,o,p,r,s,t,u,v,x,B,C,D,P}"
complete -c ksh -a "-{a,b,c,e,f,h,i,k,n,o,p,r,s,t,u,v,x,B,C,D,P}"
complete -c ksh -s R

complete -c ksh -a "$options"
