# protocentral-pulse-express

Pulse Express
================================

[Do I have to write an algorithm to calculate the Pulse-Ox (SpO2) and heart rate levels ? Not anymore, the Pulse Express does it for you.](https://protocentral.com/product/pulse-express-pulse-ox-heart-rate-sensor-with-max32664/?preview_id=10783&preview_nonce=12b54ac79b&_thumbnail_id=10790&preview=true)

![Pulse Express Pulse-Ox & Heart Rate Sensor](extras/4806.jpg)

Pulse Express is an efficient and versatile breakout board with integrated high-sensitivity optical sensors (MAX30102) and also a chip that does the calculations (biometric sensor hub MAX32664D). Integrating Maxim’s MAX32664 Version D makes Pulse Express unique, with an internal algorithm that works to measure different data as you start. With its built-in low power capability, the board is suitable for any wearable health for finger-based applications.

**Note: This device is only meant to be used for research & development purposes and is NOT to be used as a medical device. This product is not FDA, CE or FCC approved for consumer use.** 

Features:
* Integrates a high-sensitivity pulse oximeter and heart rate sensor (MAX30102).
* Integrated biometric sensor hub (MAX32664)
* In-built accelerometer for robust detection and compensation of motion artifacts(Optional).
* Easy-to-use I2C interface to connect to any host microcontroller.
* Ultra-Low Power
* Algorithms Measure:
  Pulse Heart Rate,
  Pulse Blood Oxygen Saturation (SpO2),
  Estimated Blood Pressure.
* Dimensions: 35 mm x 17  mm

Includes:
----------
* 1x ProtoCentral Pulse Express breakout
* 1x Welgro strap

Repository Contents
-------------------
* **/Software** - Arduino and Processing library and example sketches.
* **/Hardware** - All Eagle design files (.brd, .sch)
* **/extras** -   includes the datasheet


Wiring the Breakout to your Arduino
------------------------------------
Connection with the Arduino board is as follows:
 
|Pulse pin label   | Arduino Connection   |Pin Function      |
|----------------- |:--------------------:|-----------------:|
| VCC              | +5V                  |  Supply voltage  |             
| SCL              | A5                   |  Serial clock    |
| SDA              | A4                   |  Serial data     |
| GND              |                      |  Gnd             |


License Information
===================

This product is open source! Both, our hardware and software are open source and licensed under the following licenses:

Hardware
---------

**All hardware is released under [Creative Commons Share-alike 4.0 International](http://creativecommons.org/licenses/by-sa/4.0/).**
![CC-BY-SA-4.0](https://i.creativecommons.org/l/by-sa/4.0/88x31.png)

You are free to:

* Share — copy and redistribute the material in any medium or format
* Adapt — remix, transform, and build upon the material for any purpose, even commercially.
The licensor cannot revoke these freedoms as long as you follow the license terms.

Under the following terms:

* Attribution — You must give appropriate credit, provide a link to the license, and indicate if changes were made. You may do so in any reasonable manner, but not in any way that suggests the licensor endorses you or your use.
* ShareAlike — If you remix, transform, or build upon the material, you must distribute your contributions under the same license as the original.

Software
--------

**All software is released under the MIT License(http://opensource.org/licenses/MIT).**

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


Please check [*LICENSE.md*](LICENSE.md) for detailed license descriptions.

