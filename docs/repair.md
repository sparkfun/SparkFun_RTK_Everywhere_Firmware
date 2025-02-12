# Disassembly / Repair

<!--
Compatibility Icons
====================================================================================

:material-radiobox-marked:{ .support-full title="Feature Supported" }
:material-radiobox-indeterminate-variant:{ .support-partial title="Feature Partially Supported" }
:material-radiobox-blank:{ .support-none title="Feature Not Supported" }
-->

<div class="grid cards fill" markdown>

- EVK: [:material-radiobox-blank:{ .support-none }]( title ="Feature Not Supported" )
- Postcard: :material-radiobox-indeterminate-variant:{ .support-partial title="Feature Not Supported" }
- Torch: :material-radiobox-indeterminate-variant:{ .support-partial title="Feature Partially Supported" }

</div>

The RTK product line is fully open-source hardware. This allows users to view schematics, code, and repair manuals. This section documents how to safely disassemble the RTK Facet and Torch.

Repair Parts:

<div class="grid cards" markdown>

-   <a href="https://www.sparkfun.com/products/24064">
	<figure markdown>
	![Product Thumbnail](https://cdn.sparkfun.com/assets/parts/2/4/3/8/7/SPX-24064_RTK_Facet_Main_v13_-_1.jpg)
	</figure>

	---

	**SparkFun RTK Replacement Parts - Facet Main Board v13**<br>
	SPX-24064</a>

-   <a href="https://www.sparkfun.com/products/24673">
	<figure markdown>
	![Product Thumbnail](https://cdn.sparkfun.com/assets/parts/2/4/9/9/8/SPX-24673_RTK_Facet_Housing.jpg)
	</figure>		

	---

	**SparkFun RTK Replacement Parts - Facet Housing**<br>
	SPX-24673</a>

-   <a href="https://www.sparkfun.com/products/24674">
	<figure markdown>
	![Product Thumbnail](https://cdn.sparkfun.com/assets/parts/2/4/9/9/9/SPX-24674_RTK_Facet_L-Band_Housing.jpg)
	</figure>

	---

	**SparkFun RTK Replacement Parts - Facet L-Band Housing**<br>
	SPX-24674</a>

-   <a href="https://www.sparkfun.com/products/24675">
	<figure markdown>
	![Product Thumbnail](https://cdn.sparkfun.com/assets/parts/2/5/0/0/0/SPX-24675_RTK_Facet_L-Band_Main_v14_-_1.jpg)
	</figure>

	---

	**SparkFun RTK Replacement Parts - Facet L-Band Main Board v14**<br>
	SPX-24675</a>

-   <a href="https://www.sparkfun.com/products/24705">
	<figure markdown>
	![Product Thumbnail](https://cdn.sparkfun.com/assets/parts/2/5/0/2/9/SPX-24705_RTK_Facet_Display_Assembly_v12_-_1.jpg)
	</figure>		

	---

	**SparkFun RTK Replacement Parts - Facet Display/Button**<br>
	SPX-24705</a>

-   <a href="https://www.sparkfun.com/products/24706">
	<figure markdown>
	![Product Thumbnail](https://cdn.sparkfun.com/assets/parts/2/5/0/3/0/SPX-24706_RTK_Facet_Connector_Board_-_1.jpg)
	</figure>		

	---

	**SparkFun RTK Replacement Parts - Facet Connector Assembly v12**<br>
	SPX-24706</a>

-   <a href="https://www.sparkfun.com/products/24707">
	<figure markdown>
	![Product Thumbnail](https://cdn.sparkfun.com/assets/parts/1/7/6/9/8/SPX-24707_RTK_Facet_Rubber_Sock.jpg)
	</figure>		

	---

	**SparkFun RTK Replacement Parts - Facet Rubber Sock**<br>
	SPX-24707</a>

</div>

Tools Needed:

- [Small Philips Head Screwdriver](https://www.sparkfun.com/products/9146)
- [Curved Tweezers](https://www.sparkfun.com/products/10602)
- [U.FL Puller](https://www.sparkfun.com/products/20687) - *Recommended*
- [Wire Cutters](https://www.sparkfun.com/products/10447) - *Recommended*

## Facet

### Opening Enclosure

<figure markdown>
![Remove the protective silicone boot](./img/Repair/SparkFun-RTK-Repair-1.jpg)
<figcaption markdown>
</figcaption>
</figure>

Starting from the back of the unit, remove the protective silicone boot. If your boot has gotten particularly dirty from field use, rinse it with warm water and soap to clean it up.

<figure markdown>
![Remove four Philips head screws](./img/Repair/SparkFun-RTK-Repair-2.jpg)
<figcaption markdown>
</figcaption>
</figure>

Remove the four Philips head screws. They may not come all the way out of the lower enclosure.

<figure markdown>
![Lid removed](./img/Repair/SparkFun-RTK-Repair-3.jpg)
<figcaption markdown>
</figcaption>
</figure>

The top lid should then come off. The front overlay is adhesive and may adhere slightly to the 'tooth' on the lid. You will not damage anything by gently prying it loose from the lid as you lift the lid.

!!! tip
	The lid makes a great screw bin.

<figure markdown>
![Remove the antenna](./img/Repair/SparkFun-RTK-Repair-4.jpg)
<figcaption markdown>
</figcaption>
</figure>

!!! warning "Attention"
	Please, note the antenna orientation so that it can be re-mounted in the same way. A sharpie dot towards the display is a handy method.

Remove the four screws holding the antenna in place.

Be aware that the antenna material is susceptible to fingerprints. You won't likely damage the reception but it's best to just avoid touching the elements.

<figure markdown>
![Antenna to the side](./img/Repair/SparkFun-RTK-Repair-5.jpg)
<figcaption markdown>
</figcaption>
</figure>

The antenna will be attached to the main board and must stay that way for the next few steps. Without pulling on the thin RF cable, gently set the antenna to the side.

<figure markdown>
![Remove battery boat](./img/Repair/SparkFun-RTK-Repair-6.jpg)
<figcaption markdown>
</figcaption>
</figure>

The battery and vertical PCBs are held in place using a retention PCB. Remove the four screws holding the PCB in place and lift off the foam top of the battery holder.

!!! note
	v1.0 of the retention plate is not symmetrical. Meaning, if the plate is installed in reverse, the retention PCB will be just short of the connector board and will not properly hold it in place. Reinstall the retention plate as shown in the picture.

!!! note
	The foam is held to the PCB using an adhesive. Some of that adhesive is exposed to catch material that may enter into the enclosure. Try to avoid getting stuck.

<figure markdown>
![Holder PCB to the side](./img/Repair/SparkFun-RTK-Repair-7.jpg)
<figcaption markdown>
</figcaption>
</figure>

Set the retention PCB to the side.

### Removing Antenna Connection

<figure markdown>
![Removing the antenna connection](./img/Repair/SparkFun-RTK-Repair-8.jpg)
<figcaption markdown>
</figcaption>
</figure>

This is the most dangerous step. The cable connecting the antenna to the main board uses something called a U.FL or IPEX connector. These tend to be fragile. You can damage the connector rendering the unit inoperable. Just be sure to take your time.

Using the U.FL removal tool, slide the tool onto the U.FL connector and gently pull away from the main board. If it won't give, you may need to angle the tool slightly while pulling.

!!! note
	If you do not have a U.FL tool this [tutorial on U.FL connectors](https://learn.sparkfun.com/tutorials/three-quick-tips-about-using-ufl/all#disconnect) has three alternative methods using tweezers, wire cutters, and a skinny PCB that may also work.

<figure markdown>
![U.FL connector removed](./img/Repair/SparkFun-RTK-Repair-9.jpg)
<figcaption markdown>
</figcaption>
</figure>

The U.FL connector will disconnect. The antenna can now be set to the side.

### Opening Backflip Connectors

<figure markdown>
![Flipping connector](./img/Repair/SparkFun-RTK-Repair-10.jpg)
<figcaption markdown>
</figcaption>
</figure>

Many of the connections made within the RTK product line use this 'back flip' style of FPC connector. To open the connector and release the flex printed circuit (FPC) cable, use a curved pair of tweezers to gently flip up the arm. The arm in the connector above has been flipped, the FPC can now be removed.

As shown above, remove the FPC connecting to the 4th connector on the main board. The connector is labeled 'SD Display'. Leave all other FPCs in place.

<figure markdown>
![Removing main board](./img/Repair/SparkFun-RTK-Repair-13.jpg)
<figcaption markdown>
</figcaption>
</figure>

The main board is attached to the battery and the connector board. Lift the mainboard and connector board together, bringing the battery with the assembly.

### Removing the Battery

<figure markdown>
![Removing the JST connector](./img/Repair/SparkFun-RTK-Repair-14.jpg)
<figcaption markdown>
</figcaption>
</figure>

!!! note
	This step is not needed for general repair. Only disconnect the battery if you are replacing the battery.

The battery is plugged into the mainboard using a JST connector. These are very strong connectors. *Do not* pull on the wires. We recommend using the mouth of wire cutters (also known as diagonal cutters) to pry the connector sideways.

Once removed, the battery can be set aside.

### Removing the Front Overlay

<figure markdown>
![Disconnecting overlay](./img/Repair/SparkFun-RTK-Repair-15.jpg)
<figcaption markdown>
</figcaption>
</figure>

The front overlay (the sticker with the Power button) is connected to the display board using the same style 'back flip' FPC connector. Flip up the arm and disconnect the overlay.

<figure markdown>
![Peeling off the overlay](./img/Repair/SparkFun-RTK-Repair-16.jpg)
<figcaption markdown>
</figcaption>
</figure>

Gently peel off the adhesive overlay from the front face. This cannot be saved.

### Inserting New Display Board

<figure markdown>
![New display board in place](./img/Repair/SparkFun-RTK-Repair-17.jpg)
<figcaption markdown>
</figcaption>
</figure>

Slide the old display board out. Remove the brown FPC from the old display board and move over to the new display board. Insert the new display board into the slot.

<figure markdown>
![Remove film](./img/Repair/SparkFun-RTK-Repair-18.jpg)
<figcaption markdown>
</figcaption>
</figure>

With the new display board in place, remove the protective film from the display.

<figure markdown>
![Apply new overlay](./img/Repair/SparkFun-RTK-Repair-19.jpg)
<figcaption markdown>
</figcaption>
</figure>

Remove the backing from the new overlay. Stick the overlay into the center of the front face area.

<figure markdown>
![Insert tab into connector](./img/Repair/SparkFun-RTK-Repair-20.jpg)
<figcaption markdown>
</figcaption>
</figure>

Be sure to flip up the arm on the overlay connector before trying to insert the new overlay FPC.

Using tweezers, and holding the FPC by the cable stiffener, insert the overlay FPC into the display board.

### Closing The Backflip Connector

<figure markdown>
![Closing the arm on an FPC](./img/Repair/SparkFun-RTK-Repair-21.jpg)
<figcaption markdown>
</figcaption>
</figure>

Use the nose of the tweezers to press the arm down, securing the FPC in place.

<figure markdown>
![FPC in the display board connector](./img/Repair/SparkFun-RTK-Repair-22.jpg)
<figcaption markdown>
</figcaption>
</figure>

If you haven't already done so, move the brown FPC from the original display board over to the new display board. Be sure to open the connector before inserting the FPC, and then press down on the arm to secure it in place.

### Reinstalling Main Board

<figure markdown>
![Returning boards into place](./img/Repair/SparkFun-RTK-Repair-23.jpg)
<figcaption markdown>
</figcaption>
</figure>

Slide the main board and connector boards back into place along with the battery. We find it easier to partially insert the connector board, then the main board, and then adjust them down together.

<figure markdown>
![Handling FPCs](./img/Repair/SparkFun-RTK-Repair-11.jpg)
<figcaption markdown>
</figcaption>
</figure>

Reconnect the display board to the main board. Be sure to close the arm on the main board to secure the FPC in place.

### Testing the Overlay

<figure markdown>
![Internal Power Button](./img/Repair/SparkFun-RTK-Repair-24.jpg)
<figcaption markdown>
</figcaption>
</figure>

The RTK Facet has two power buttons: the external button on the overlay and an internal button on the back of the display board (shown above). Pressing and holding the internal button will verify the connection between the display board and the main board.

If the internal button is not working, remove and reinsert the FPC connecting the display board to the main board.

Press and hold the internal power button to power down the unit.

<figure markdown>
![Using overlay power button](./img/Repair/SparkFun-RTK-Repair-26.jpg)
<figcaption markdown>
</figcaption>
</figure>

Repeat the process using the overlay button to verify the external power button is working.

If the external overlay button is not working, but the internal button is, remove and reinsert the FPC connecting the overlay to the display board.

If the external button is working, proceed with re-assembling the unit.

### Reassembly

Confirm that all FPC armatures are in the down and locked position.

<figure markdown>
![Attach U.FL connector](./img/Repair/SparkFun-RTK-Repair-9.jpg)
<figcaption markdown>
</figcaption>
</figure>

Carefully line the U.FL connector up with the main board and gently press the connector in place. A tool is useful in this step but an index finger works just as reliably.

<figure markdown>
![Protect the battery](./img/Repair/SparkFun-RTK-Repair-5.jpg)
<figcaption markdown>
</figcaption>
</figure>

Place the retention plate and foam over the battery. The battery may need to be nudged slightly to align with the upper cavity.

!!! note
	v1.0 of the retention plate is not symmetrical. Meaning, if the plate is installed in reverse, the retention PCB will be just short of the connector board and will not properly hold it in place. Reinstall the retention plate as shown in the picture above.

Secure the retention plate with the four *small* screws.

<figure markdown>
![Reattach antenna](./img/Repair/SparkFun-RTK-Repair-4.jpg)
<figcaption markdown>
</figcaption>
</figure>

Place the antenna over top of the retention plate in the same orientation as it was removed. Secure in place with the four *large* screws.

<figure markdown>
![Dome showing the front tooth](./img/Repair/SparkFun-RTK-Repair-3.jpg)
<figcaption markdown>
</figcaption>
</figure>

Plate the dome over the antenna with the front 'tooth' aligning over the display.

<figure markdown>
![Insert four screws into dome](./img/Repair/SparkFun-RTK-Repair-2.jpg)
<figcaption markdown>
</figcaption>
</figure>

Secure the dome in place using four *small* screws.

Replace the silicone boot around the device.

Power on the RTK Facet and take outside to confirm SIV reaches above ~20 satellites and HPA is below ~1.0m.

## Torch

### Opening Enclosure

The RTK Torch can be opened by removing four Phillips head screws located on the bottom of the enclosure.

<figure markdown>
![Antenna covered removed from the RTK Torch](./img/Repair/GPS-24672-RTK-Torch-Internal2.jpg)
<figcaption markdown>
Antenna covered removed from the RTK Torch
</figcaption>
</figure>

### Removing Antenna Stackup

Once the antenna cover is removed, remove the three Phillips holding the antenna in place.

With the screws removed, gently and very carefully pull up on the upper PCB antenna. There is an MMCX connector that will pop loose, along with a U.FL connection. Pulling too hard may damage the connectors, the PCB antennas, or both.

### Removing Mainboard

<figure markdown>
![The RTK Torch mainboard](./img/Repair/GPS-24672-RTK-Torch-Internal1.jpg)
<figcaption markdown>
The RTK Torch mainboard
</figcaption>
</figure>

With the mainboard removed, three Phillips head screws hold the mainboard in place. Remove these screws and the mainboard will be released. Remove the battery connector from the bottom side of the mainboard to free it.

### Remoing Battery

The lower 7.2V LiPo battery pack is held in place by a metal retaining plate. Remove the Phillips head screws to release the battery.
