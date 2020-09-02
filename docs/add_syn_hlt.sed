# left-right angular brackets escape (HTML)
s/</\&lt;/g
s/>/\&gt;/g

# Keywords
s/\<fn\>/<span class=k>fn<\/span>/g
s/\<pub\>/<span class=k>pub<\/span>/g

# Built-in Types
s/\<u8\>/<span class=t>u8<\/span>/g

# Functions
s/\([a-zA-Z_][a-zA-Z0-9_]*\)(/<span class=f>\1<\/span>(/g

# Numbers
s/\b\([0-9]\+\(\.[0-9]\+\)\?\)\b/<span class=n>\1<\/span>/g

# Strings
s/\("[^"]*"\)/<span class=s>\1<\/span>/g

# Comments
s/\(\/\/.*\)/<span class=c>\1<\/span>/g

# \# directive
s/\(#[a-zA-Z_][a-zA-Z0-9_]*\)/<span class=i>\1<\/span>/g

# import path (absolute)
s/\(\&lt;.*\&gt;\)/<span class=ia>\1<\/span>/g
