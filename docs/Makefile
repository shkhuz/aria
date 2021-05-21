MD_FILES := $(shell find . -name "*.md")
HTML_FILES := $(addsuffix .html, $(basename $(MD_FILES)))

all: $(HTML_FILES)

%.html: %.md
	pandoc -s $^ -c style.css --template pandoc_template.html --toc --toc-depth=3 -o $@

clean: 
	rm $(HTML_FILES)

.PHONY: clean
