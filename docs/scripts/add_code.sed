s/@\(.*\.ar\)/cat examples\/\1.html/ge
s/\$\([0-9]\+,[0-9]\+\)\ \([^\.]*\.ar\)/sed -n \1p examples\/\2.html/ge
