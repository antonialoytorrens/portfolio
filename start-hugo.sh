#!/bin/bash
# Start hugo server by rendering all pages and allowing outside IP
#hugo server --disableFastRender --bind $(hostname -I | awk '{print $NF}')
hugo server --disableFastRender
