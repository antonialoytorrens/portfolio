+++
author = "Antoni Aloy Torrens"
title = "Modern Linux on the Asus Eee Pad Transformer TF101"
subtitle = "Installing modern Linux on the Asus Eee Pad Transformer TF101"
date = "2022-02-03"
description = "A comprehensive guide installing modern Linux on the Asus Eee Pad Transformer TF101"
tags = [
    "linux",
    "postmarketos",
    "linux-mobile"
]
+++

Fa cosa de tres anys vaig voler experimentar amb la distribució PostmarketOS, que és una distribució Linux preparada per a telèfons mòbils, tauletes i altres dispositius encastats. Em va sorprendre molt per la qualitat que tenen les eines de desenvolupament i perquè els propis mantenidors t'ho posen molt fàcil a la hora d'aprendre i contribuir.

### Una mica de context abans

El primer dispositiu el qual vaig voler experimentar va ser la Asus Eee Pad Transformer TF101. Lo darrer que havia provat era [Lubuntu 14.04](https://sourceforge.net/projects/tf101-linux-images/files/%5BLinux%5D%20%5BIMG%5D%20%5BDev%5D%20%5BWIP%5D%20Ubuntu%20images%20for%20Rootbind%20%5BTF101-TF101G%5D/) (que després vaig voler adaptar a [Debian 8](https://github.com/antonialoytorrens/TF101-linux-images/blob/b23eaf0/README.rootbind-method.old.md) perquè l'EOL de la distribució era una mica posterior), amb el [kernel oficial d'Nvidia](https://web.archive.org/web/20240127214046/https://developer.nvidia.com/linux-tegra-rel-16) que va treure per als dispositius amb Tegra 2, amb [millores fetes per la comunitat d'XDA](https://web.archive.org/web/20231230153805/https://xdaforums.com/t/jhinta-kernel-for-lilstevie-ubuntu.1683145/) de temps enrere. Anava bé, tenia suport OpenGLES 2.0 quasi complet (a l'igual que Android de fàbrica), amb l'únic inconvenient que notava que la temperatura de la tauleta era molt alta, segurament per l'augment de freqüència de rellotge (a partir d'ara, *overclocking*) que tenia posat.

### Anem a provar una distribució distinta

Ja que el nucli s'estava quedant antic, implicant que la tauleta es quedàs desfassada en quant a actualitzacions i programari, volia mirar si hi podria haver alguna manera de tenir programari més actual (actualitzar a Debian 9 es necessitava el kernel 3.2 i el kernel proporcionat per Nvidia duia mils de línies de codi extra, cosa que adaptar allò seria molt costós). La ROM d'[Android 6.0.1](https://web.archive.org/web/20210511230457/https://public.timduru.org/Android/tf101/KatKiss/) inoficial podria aguantar un parell d'anys més, a base d'instal·lar F-Droid, sense serveis de Google (molta memòria en segon pla per 1GB de RAM que té) i prioritzant la rapidesa i lleugeresa de l'aplicació davant de l'usabilitat.

Va ser quan vaig trobar la distribució PostmarketOS, a través de la web de [TuxPhones.com](https://many.tuxphones.com/) i introduïnt el nom de TF101.
El kernel era el mateix, 3.1.15, però sense la part d'*overclocking*. Vaig veure que la tauleta s'encalentia molt menys i que també podia executar programari relativament nou (en aquell moment, Alpine 3.12). Això va ser perquè el requeriment de kernel 3.2+ que demanava Debian 9 era per la versió de la llibreria C (glibc). Alpine Linux empra musl libc, una altra llibreria de C molt més reduïda en tamany, i que no demanava tal requeriment de versió de nucli mínima (si no vaig errat és 3.0+ per a totes les versions per a certes cridades kernel a musl i a l'inrevés, cosa que aquest kernel complia).

### I ara?

