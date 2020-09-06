s/@(.*\.ar)/cat examples\/\1.html/ge
s/\$([0-9]+,[0-9]+)\ ([^\.]*\.ar)/sed -n \1p examples\/\2.html/ge
s/#(((-e\ [0-9]+(,[0-9]+)?p\ )+)([^\.]*\.ar))/sed -n \2 examples\/\5.html/ge
