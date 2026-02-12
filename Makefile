.PHONY: help test run

help:
	@echo "Targets:"
	@echo "  make test      - run unit + e2e tests (ctest) (reads: repo, writes: build-tests/)"
	@echo "  make run FEED= OUT= - run gtfs2rdf on FEED and write output to OUT (reads: FEED, writes: OUT)"
	@echo ""
	@echo "Example:"
	@echo "  make run FEED=/extern/local/feeds/myfeed.zip OUT=/extern/local/out.ttl"

test:
	cd /opt/gtfs2rdf && ctest --test-dir build-tests --output-on-failure

run:
	@test -n "$(FEED)" || (echo "ERROR: FEED is not set"; exit 1)
	@test -n "$(OUT)"  || (echo "ERROR: OUT is not set"; exit 1)
	/opt/gtfs2rdf/build-release/gtfs2rdf "$(FEED)" --output "$(OUT)"

