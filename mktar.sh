#!/bin/bash

git archive --format=tar.gz --prefix=pvr.iptvsimple-1.1.0-$(git rev-parse --short HEAD)/ $(git rev-parse --short HEAD) > pvr.iptvsimple-1.1.0-$(git rev-parse --short HEAD).tar.gz
