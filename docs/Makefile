AR_FILES := $(shell find . -name "*.ar")
AR_HTML_FILES := $(addsuffix .html, $(AR_FILES))

install: $(AR_HTML_FILES)
	sed 's/@\(.*\.ar\)/cat \1.html/ge' index.raw_html > index.html

%.ar.html: %.ar
	sed -f add_syn_hlt.sed $^ > $@

clean:
	rm -f index.html
	rm -f examples/*.ar.html

