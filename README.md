
# ![alt text](resources/Images/Logo.jpg "BitTorrent") BIT TORRENT


[icon]: https://github.com/sferik/t/raw/master/icon/t.png
## Introduction:
- BitTorrent is an application layer network protocol used to distribute files. 
- It uses a peer-to-peer (P2P) network architecture where many peers act as a client and a server by downloading from peers at the same time they are uploading to others.
-  The serving capacity increases as the number of downloaders increases making the system self-scaling. 
- It also uses a client-server architecture where peers contact the server to find other peers that they may connect to. 

## Architecture:
The BitTorrent architecture normally consists of the following entities
- a static metainfo file (a “torrent file”)
- a ‘tracker’
- an original downloader (“seed”)
- the end user downloader (“leecher”) 