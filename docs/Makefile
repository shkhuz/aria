MD_FILES := $(shell find . -name "*.md")
HTML_FILES := $(addsuffix .html, $(basename $(MD_FILES)))

all: $(HTML_FILES)

run: 
	http-server -p 4000 ./

%.html: %.md
	pandoc -s $^ -c style.css --template pandoc_template.html --toc --toc-depth=3 -o $@

clean: 
	rm -f $(HTML_FILES)

.PHONY: clean
