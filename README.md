# Pye Collection Manager (PCM)
A tool for speeding sorting of cards between collection, trades and trash

# Requirements

* An android phone running ScryGlass (Settings -> Advanced -> Custom On-Scan URL -> your box with a 8080 port)

* A PC/Laptop running PCM (Windows/Linux/Mac)

* AllSetsArray.json from mtgjson.com

* CSV file of card in your collection (Currently targeting the export from DeckBox.org)

* CSV file of all cards with prices (Currently targeting the export from DeckBox.org)

* That your happy to trash cards under 10c of value and that you only want to keep 1 copy of every card.

# Compiling

Using a reasonably recent Qt, something in the mid 5.7+ is required I believe (I personally develop with 5.7, although 5.6 might work, but Ubuntu's 5.5 doesn't), run the following in the project directory:

qmake
make

# Running

Execute the following to start the program:

./PyeCollectionManager

Check the second tab for important directories & other operations.

If no sounds play try installing all the gstreamer0.1 plugins, at least that is what the Qt binary packages require. If using the ubuntu packages in a future Ubuntu, gstreamer1.0 might be used. For gstreamer0.1 the final thing that got stuff working for me was:

sudo apt-get install libgstreamer0.10-dev libgstreamer-plugins-base0.10-dev

But it is plausible that some of the previous similar things I did are also required.

There are a couple of files that PCM requires for data sources:

## JSON Oracle Source

Download http://mtgjson.com/json/AllSetsArray.json and place it somewhere accessible, this file is the information about cards that is required. Without this file the program will essentially do nothing.

When a new set is release (shortly after it is released to gatherer) you can download a new mtgjson file to get support for the new cards.

## Deckbox Prices

If you want the sorting between Trash and Trades, PCM needs to be supplied with card prices. The best way to do this is run the program and then use the option on the second tab to generate a "Full Card List for Deckbox". 

Import that full card list CSV file into a deckbox inventory and then export the deckbox inventory (adding all the extra columns). That file is then used by PCM to reference card values. Export that file as often as you want up to date prices. Repeat the steps of generating the "Full Card List for Deckbox" after every new set is released to ensure that you have prices for the new cards.

## Deckbox Inventory

Exporting your Deckbox inventory will allow PCM to know about the cards you already have sorted and inventoried, decisions of what to keep for your collection will use this knowledge. Exporting with all extra columns is required (the URL isn't, but just enable them all)

# Outputs

* CSV file to import to Deckbox of the new cards for your collection


# TODO

* CSV file to import to pucatrade of the new cards for your trading

* White list of cards to keep all coppies of

* White list of cards to never trash, maybe including all reserved list cards (if they can't print more we shouldn't destroy them)

