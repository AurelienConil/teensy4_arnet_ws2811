
# Petites Led

1. Led power . Clignotement indique la teensy est fonctionnement normal

2. Arnet . S'allume 1 sec a chaque message Arnet. S'eteint  1sec après l'arret des messages arnet.


# Explication du fichier carte micro sd

La carte doit etre formatté en FAT32

La carte contient un fichier configteensy.json

```

{
    "isdhcp": true,
    "ip": [
        192,
        168,
        0,
        34
    ],
    "broadcast": [
        192,
        168,
        0,
        255
    ],
    "mac": [
        4,
        13,
        100,
        0,
        82,
        13
    ],
    "issync": true,
    "arduinopins": [
        2,
        7
    ],
    "ledsperline": 59,
    "numberoflines": 5,
    "startuniverse": 7,
    "numstrips": 2


}

```


## Explications

isDHCP : true  = adresse IP assignée par le routeur
isDHCP : false = adresse IP assignée par la valeur suivante "ip" : [
]

broadcast : [] = adresse IP pour renvoyer le node poll
mac = adresse MAC de la prise ethernet . Mettre une adresse différente entre chaque boitier pour les différencier
"issync" = true/false utilisation du protocole de syncronisation arnet. Réglage a effectuer dans madmapper conjointement
"arduinopins": [] . Réglage liés aux pins du teensy utiliser. Ne pas changer sauf si changement sur l'electronique
"ledsperline": 59 . Nombre de leds sur une "ligne"
"numberoflines": 5 . Nombre de ligne branché sur chacune des sorties
"startuniverse": 7 . Univers de démarrage
"numstrips": 2 . Nombre de sorties. Doit matcher la quantité de "arduinopins"


## Calcul des univers

Au démarrage du programme, le code calcul le nombre d'univers utilisés, et donc renvoyer dans le node poll

```

  //Note : numberofstrips equivaut à numstrips
  numberofleds = ledsperline * numberoflines * numberofstrips;

  numberofchannels = numberofleds * 3;


   // +1 si la division entire n'est pas égale a 0. 
  config.numberofuniverses = config.numberofchannels / 512 + ((numberofchannels % 512) ? 1 : 0); 


  config.maxuniverses = config.startuniverse + config.numberofuniverses;
  
```





Boitier 1 : Prise ehtneret qui flotte : fichier configteensy1.json ( a renommer en configteensy.json sur la carte SD)
Boitier 2 : Prise ehtneret qui tient bien : fichier configteensy2.json ( a renommer en configteensy.json sur la carte SD) 