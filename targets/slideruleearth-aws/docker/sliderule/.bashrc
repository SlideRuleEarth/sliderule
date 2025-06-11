parse_git_branch() {
     git branch 2> /dev/null | sed -e '/^[^*]/d' -e 's/* \(.*\)/ (\1)/'
}

PS1="\n\[\033[01;32m\]\u@\h\[\033[00m\]\[\033[33m\] [\$CONDA_DEFAULT_ENV]\$(parse_git_branch):\[\033[01;34m\]\w\[\033[00m\]\n\$ "

alias bd="cd $ROOT/targets/slideruleearth-aws"
alias pc="cd $ROOT/clients/python"
alias cores="ls /var/lib/systemd/coredump/"

git config --global --add safe.directory $ROOT
git config --global user.name $GITNAME
git config --global user.email $GITEMAIL