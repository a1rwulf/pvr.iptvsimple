#!/bin/bash

git archive --format=tar.gz --prefix=pvr.iptvsimple-2.2.0-$(git rev-parse --short HEAD)/ $(git rev-parse --short HEAD) > pvr.iptvsimple-2.2.0-$(git rev-parse --short HEAD).tar.gz
