# Portfolio
My personal portfolio, made with blogc.

## Tips
### Autorun make every time there's a change
To run `make` automatically when there is a change of one of my files, I make use of [watchexec](https://github.com/watchexec/watchexec/releases/tag/v1.20.6), and run the following:
`watchexec -e mk,txt,js,css,html,tmpl,gif,png,jpg,webp,ico make`

Files in `.gitignore` get completely ignored.

### Image optimization
In order to optimize the images, I use `imagemagick` and `gifsicle` to convert jpg images into gif and optimize them:

```
#!/usr/bin/env bash
SIZE_LIMIT=14336   # 14 KB in bytes
COLORS=16
img="test.jpg"
base=$(basename "$img" .jpg)
out="$DST_DIR/${base}.gif"

# I could use Triangle filter in order to further reduce image size, but this is enough
convert "$img" -filter Lanczos -resize 200x200 -strip -colors $COLORS -dither None "$out"
gifsicle -O3 --colors $COLORS --lossy=200 "$out" -o "$out"

echo "$base.gif is $(($size/1024)) kB with $colors colors."
```

#### Any reasoning about the image size limit? Why is it 14kB?

Just to test a proof of concept shown in this article: https://endtimes.dev/why-your-website-should-be-under-14kb-in-size. 

To summarize it a little bit, due to the nature of TCP and QUIC, the minimum size of a packet is 1500 bytes (-16 bytes for IP and -24 bytes for TCP), which leaves us with 1460 bytes per packet. Having a website page served in one packet greatly improves performance since we do not have to request the server for more packets.
