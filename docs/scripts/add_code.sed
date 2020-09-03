s/@\(.*\.ar\)/cat examples\/\1.html/ge
s/\$\([0-9]\+,[0-9]\+p\)\ \([^\.]*\.ar\)/sed -n \1 examples\/\2.html/ge
