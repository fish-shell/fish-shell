set -l options allexport notify noclobber errexit noglob trackall keyword monitor noexec privileged stdin nounset verbose markdirs xtrace
complete -c oksh -a "+{a,b,C,e,f,h,i,k,l,m,n,p,r,u,v,X,x,o}"
complete -c oksh -a "-{a,b,C,e,f,h,i,k,l,m,n,p,r,u,v,X,x,o}"
complete -c oksh -s c
complete -c oksh -s s

complete -c oksh -a "$options"
