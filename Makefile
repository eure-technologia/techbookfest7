.PHONY: build
build:
	@REVIEW_CONFIG_FILE=config.yml ./build-in-docker.sh

build-ebook:
	@REVIEW_CONFIG_FILE=config-ebook.yml ./build-in-docker.sh
