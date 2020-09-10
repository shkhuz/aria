AR_FILES := $(shell find . -name "*.ar")
AR_HTML_FILES := $(addsuffix .html, $(AR_FILES))

install: index.html

index.html: $(AR_HTML_FILES) index.raw_html
	sed -r -f scripts/add_code.sed index.raw_html > index.html

%.ar.html: %.ar
	sed -f scripts/add_syn_hlt.sed $^ > $@

clean:
	rm -f index.html
	rm -f $(AR_HTML_FILES)

