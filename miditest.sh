#!/bin/bash
# controlla che la porta midi sia connessa
echo "Nota: questo script richiede 'sendmidi', reperibile su https://github.com/gbevin/SendMIDI"
A=$(sendmidi list | grep "Arduino")
if [ -z "$A" ]; then
  echo "Porta MTC non trovata"
  exit 1
fi
echo "MTC Device: $A"
read -p "Premi INVIO per inviare il comando DISPLAY SIGNATURE..."
sendmidi dev $A cc 103 1

echo "SIGNATURE: Il formato e: numeratore * 8 + denominatore come potenza di 2"
echo "Esempio: 4/4 = 4*8 + 2 = 34"
echo "Impostazione signature a 7/16 ($SIGNATURE)"
read -p "Premi INVIO per settare la signature a 7/16..."
sendmidi dev $A cc 102 $SIGNATURE
SIGNATURE=$((7*8 + 4))
sendmidi dev $A cc 102 $SIGNATURE
read -p "Premi INVIO per settare la signature a 4/4..."
SIGNATURE=$((4*8 + 2))
sendmidi dev $A cc 102 $SIGNATURE
read -p "Premi INVIO per inviare il comando RESET..."
sendmidi dev $A cc 103 0