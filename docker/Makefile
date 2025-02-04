RELEASE?=1.0.0
PLATFORM?=linux/arm64
IMAGE=base

all: image

image:
	docker buildx build -t "platinumcd/plugin-cwaggle-base:$(RELEASE)-$(IMAGE)" --load .

push:
	docker buildx build -t "platinumcd/plugin-cwaggle-base:$(RELEASE)-$(IMAGE)" --platform "$(PLATFORM)" --push .

push-debug:
	docker buildx build -t "platinumcd/plugin-cwaggle-base-debug:$(RELEASE)-$(IMAGE)" --platform "$(PLATFORM)" -f debug.Dockerfile --push .
