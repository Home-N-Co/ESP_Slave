# Système de Gestion de Feux Tricolores Connectés

## Description

Ce projet implémente un système intelligent de contrôle de feux tricolores utilisant un ESP32 connecté à Adafruit IO. Le système permet de gérer les feux de circulation en fonction du trafic détecté, des passages piétons et des véhicules prioritaires.

## Fonctionnalités

- **Configuration WiFi par interface web** : L'ESP32 crée un point d'accès permettant à l'utilisateur de configurer les identifiants WiFi et Adafruit IO
- **Communication MQTT** : Connexion avec Adafruit IO pour recevoir les données de trafic en temps réel
- **Gestion de plusieurs paramètres** :
    - Détection de véhicules standards
    - Détection de véhicules prioritaires (urgence)
    - Détection de piétons
    - Minuterie pour la gestion des cycles
- **Stockage persistant** : Utilisation de la mémoire non-volatile pour conserver les identifiants

## Prérequis

### Matériel
- ESP32
- Connexion à Internet

### Logiciels et bibliothèques
- Arduino IDE ou PlatformIO
- Bibliothèques :
    - WiFi
    - ESPAsyncWebServer
    - Preferences
    - HTTPClient
    - PubSubClient
    - ArduinoJson

### Service Cloud
- Compte Adafruit IO avec les feeds configurés:
    - `[direction].traffic`
    - `[direction].priority`
    - `[direction].pedestrian`
    - `[direction].timer`

## Installation

1. Clonez ce dépôt
2. Installez les bibliothèques nécessaires dans votre environnement de développement
3. Compilez et téléversez le code sur votre ESP32

## Configuration initiale

Lors du premier démarrage, l'ESP32 crée un point d'accès WiFi nommé "ESP32-Access-Point" avec le mot de passe "123456789". Connectez-vous à ce réseau pour accéder à la page de configuration à l'adresse 192.168.4.1.

Vous devrez renseigner :
- SSID et mot de passe WiFi
- Nom d'utilisateur Adafruit IO
- Clé API Adafruit IO
- Direction du feu de circulation (ex: "north", "south", etc.)

## Structure du projet

Le projet est organisé autour des fonctions principales suivantes :
- `Wifi_Setup()` : Connexion au réseau WiFi
- `post_Setup()` : Envoi des données à Adafruit IO via HTTP POST
- `get_Setup()` : Récupération des données depuis Adafruit IO via HTTP GET
- `start_access_point()` : Création du point d'accès pour la configuration
- `detectVehicle()` : Détection et signalement d'un véhicule
- `detectPedestrian()` : Détection et signalement d'un piéton
- `controlTrafficLight()` : Logique de contrôle des feux de circulation

## Utilisation

Une fois configuré, l'ESP32 se connecte automatiquement au WiFi et à Adafruit IO. Il commence à surveiller les différents feeds MQTT pour réagir aux changements de trafic et aux événements.

## Dépannage

- **Échec de connexion WiFi** : L'ESP32 redémarrera automatiquement en mode point d'accès après 10 secondes si la connexion WiFi échoue
- **Problèmes avec Adafruit IO** : Vérifiez que votre clé API et votre nom d'utilisateur sont corrects
- **Messages d'erreur HTTP** : Consultez les logs série pour identifier les erreurs de communication avec l'API

## Contributeurs

- [@deBarbarinAntoine](https://github.com/deBarbarinAntoine)
- [@Fabibi47](https://github.com/Fabibi47)
- [@KANTIN-FAGN](https://github.com/KANTIN-FAGN)
- [@Nicolas13100](https://github.com/Nicolas13100)
- [@Plcuf](https://github.com/Plcuf)
- [@VitoDeriu](https://github.com/VitoDeriu)