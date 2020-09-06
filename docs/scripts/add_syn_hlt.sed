# left-right angular brackets escape (HTML)
s/</\&lt;/g
s/>/\&gt;/g

# Keywords
s/\<fn\>/<span class=k>fn<\/span>/g
s/\<pub\>/<span class=k>pub<\/span>/g
s/\<struct\>/<span class=k>struct<\/span>/g
s/\<Self\>/<span class=k>Self<\/span>/g
s/\<shared\>/<span class=k>shared<\/span>/g
s/\<mod\>/<span class=k>mod<\/span>/g

# Built-in Types
s/\<u8\>/<span class=t>u8<\/span>/g
s/\<u16\>/<span class=t>u16<\/span>/g
s/\<u32\>/<span class=t>u32<\/span>/g
s/\<u64\>/<span class=t>u64<\/span>/g
s/\<i8\>/<span class=t>i8<\/span>/g
s/\<i16\>/<span class=t>i16<\/span>/g
s/\<i32\>/<span class=t>i32<\/span>/g
s/\<i64\>/<span class=t>i64<\/span>/g
s/\<f32\>/<span class=t>f32<\/span>/g
s/\<f64\>/<span class=t>f64<\/span>/g

# Function Calls
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
