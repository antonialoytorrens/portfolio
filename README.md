# Portfolio

CV d'Antoni Aloy Torrens, convertit a un lloc estàtic amb [Hugo](https://gohugo.io/).

Plantilla original: [Resume](https://github.com/StartBootstrap/startbootstrap-resume) de StartBootstrap (Bootstrap 4).

Idiomes: **castellà (es)**, **català (ca)** i **anglès (en)**. L'idioma per defecte és el castellà.

## Requisits

Hugo Extended 0.120 o superior.

## Makefile

```sh
make help      # llista d'objectius
make           # construeix a public/
make minify    # construcció de producció (minificada)
make serve     # previsualitza a http://127.0.0.1:1313/
make draft     # igual, incloent esborranys
make clean     # esborra public/ i recursos generats
make rsync     # puja public/ a portfolio:/var/www/portfolio
make deploy    # minify + rsync
```

`make serve` obre `http://127.0.0.1:1313/` (redirigeix a `/es/`).

El `rsync` sincronitza el directori `public` cap a `portfolio:/var/www/portfolio` amb
`-avE --progress --delete`. El host `portfolio` ha d'estar definit a `~/.ssh/config`.

## Contingut

- Textos de la UI: `i18n/{es,ca,en}.yaml`
- Projectes: `data/projects.yaml`
- Imatges de projecte: `assets/projects/` (Hugo les retalla a 440×440)
- CSS, JS i icones: `static/`
