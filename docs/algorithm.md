Autocrop Algorithm for autoCropScribe.c
=======================================

* <a href="#open_reduced_image">Open reduced image.</a>
* <a href="#convert_to_gray">Convert reduced image to grayscale.</a>
* <a href="#calculate_bitonal_thresh">Calculate initial bitonalization threshold.</a>
* <a href="#bitonalize">Bitonalize reduced image.</a>
* <a href="#remove_black_border">Remove black border from top, bottom, and outer edges.</a>
* <a href="#find_binding_edge">Find binding edge and binding deskew angle.</a>
* <a href="#find_binding_thresh">Find binding threshold.</a>
* <a href="#calculate_initial_cropbox">Calculate initial cropbox.</a>
* <a href="#open_fullsize_image">Open fullsize image.</a>
* <a href="#convert_to_gray_full">Convert fullsize image to grayscale.</a>
* <a href="#bintonalize_full">Bitonalize image using binding bitonalization threshold.</a>
* <a href="#find_text_skew">Calculate text deskew angle.</a>
* <a href="#skew_mode">Determine skew mode and deskew angle.</a>
* <a href="#deskew">Deskew fullsize rotated grayscale image.</a>
* <a href="#refine_outer_edge">Refine outer edge calculation.</a>
* <a href="#find_clean_lines">Find clean crop lines.</a>
* <a href="#write_output">Write output.</a>



<a name="open_reduced_image">Open reduced image.</a>
--------------------------------------------------------------------------------

Initial cropbox parameters are calculated using the reduced-resolution image.

We decompress a 1/8 reduced-resolution image from the original JPEG, by
calling Leptonica's pixReadStreamJpeg, which passes a scaling factor to libjpeg:

    pixReadStreamJpeg(fp, 0, 8, NULL, 0)

Originally, we used the reduced image for performance reasons (realtime mode),
but the current algorithm later opens the full-resolution JEPG for cropbox
refinement, so we could open the full JPEG initially and then downsample for
initial calculations.

Since most book pages have portrait page orientation and our digital cameras
have landscape orientation, we mount the cameras sideways to maximize PPI.
This means that we will have to rotate the image by 90 degrees, using the
rotation parameter passed into the autocrop program.

We use "1" to indicate that the page should be rotated clockwise, and "-1" to
indicate counter-clockwise rotation. We use "0" to indicate foldout pages,
which are shot correctly, since theygenerally have landscape page orientation.


<a name="convert_to_gray">Convert reduced image to grayscale.</a>
--------------------------------------------------------------------------------

Color-to-Grayscale conversion is calculated in `ConvertToGray()`, which uses
either a single RGB channel or all three to produce the grayscale image.

For most pages, standard RGB->YUV weighting is used for three-channel grayscale
conversion (wR=0.3, wG=0.6, wB=0.1). However, for some pages, including covers
of library-bound books, the standard grayscale conversion does not produce a
grayscale image with enough range to produce a high-quality bitonalized version.
For these images, only one channel is used to proudce the grayscale image.

We first calculate the histogram of pixel values for each of the RGB channels,
and then find the peak of each histogram.

If the largest of the three historgram peak values is more than twice the
value of the second largest peak (MAGIC_NUMBER), then the RGB channel with 
the maximum peak is used as the gray channel.

Otherwise, all three channels are used to compute the gray channel, using
standard weighting.


<a name="calculate_bitonal_thresh">Calculate initial bitonalization threshold.</a>
--------------------------------------------------------------------------------

We calculate a bitonalization threshold in `CalculateTreshInitial()` by
analyzing the grayscale histogram.

The histogram usually contains two peaks: one peak represents the white pixels
of the page and the other represents the black text, as well as the black
background of the scanner.

The bitonalization threshold is calculated based on the peak that represents
the white page, which is the usually the larger of the two peaks. However,
for small pages, the black background takes up a considerable amount of the
image, making the black peak larger than the white peak.

Therefore, we look at the peak in the "brightest part" of the histogram, and set
the bitonalization threshold to be the pixel value that corresponds to 
20% (MAGIC_NUMBER) of the peak (on the darker side).

The "brightest part" of the histogram is defined as the the part of the
histogram that lies between the brightest pixel and a point halfway between the 
brightest and darkest pixels in the image.

For some images with no clear peak, a threshold corresponding to 20% of the
peak can't be found in the brightest part of the histogram. For these images,
we find the pixel value corresponding to the largest value in the bright part
of the histogram and set the threshold to be half that pixel value. (MAGIC_NUMBER)

For plain black images, the "brightest part" of the histogram includes the
entire range, resulting in a threshold of zero. For these images, we return
a bitonalization threshold of 140 (MAGIC_NUMBER).


<a name="bitonalize">Bitonalize reduced image.</a>
--------------------------------------------------------------------------------

We bitonalize the image using simple thesholding.


<a name="remove_black_border">Remove black border from top, bottom, and outer edges.</a>
--------------------------------------------------------------------------------

For the three non-binding edges, we initially set the crop lines to be the
edge of the image, and then we move the crop lines in towards the center.
This is done in the `RemoveBackground{Top,Bottom,Outer}` functions.

For each side of the reduced image, we sweep a kernel that is 60% the length of
the side and one pixel wide (MAGIC_NUMBER). We center the kernel on the image
edge and narrow the crop box by one pixel for each line encountered for which
90% of the pixels inside the kernel are black (MAGIC_NUMBER).

We stop the sweep on the first line that is not 90% black. This sets initial
crop lines for three sides.


<a name="find_binding_edge">Find binding edge and binding deskew angle.</a>
--------------------------------------------------------------------------------

The binding is very distinct in these images. The Scribe scanner glass platen is
used to press the pages flat. Since the book is held open at an angle, the
platen is comprised of two glass plates that form a "V" shape, and the seam
where the two plates meet sits on top of the book binding. This seam shows up
black in the image, which helps accentuate the binding.

However, the acutal binding may not lie exactly under the platen seam. Also,
due to irregularities in the binding, the binding may not be perfectly vertical.

In order to determine the binding location we use a Sum of Absolute Differences
(SAD) metric to find the vertical line that shows the largest difference between
dark pixels (the binding) and bright pixels (page white). This is done in the
function `FindBindingEdge2()`.

Since the binding may not be perfectly vertical, we rotate the image between
-1 degree and +1 degree, in 0.05 degree increments. For each rotation,
we find the vertical line that gives us a maximum SAD value for a vertical
kernel. We use a kernel that is 80% of the height of the reduced grayscale
image, which we sweep in from the edge of the image to 10% of the image width
(MAGIC_NUMBER).

The line that maximizes SAD might correspond to either the left or right side
of the binding edge, so we are careful to adjust the binding location to the
correct side of the binding.

This algorithm indicates both the location of the binding, as well as the
rotation angle to straighten the binding. It is important to note that due
to problems with gluing or stiching the binding, it may not be perfectly
parallel to the outer book edge, and it may not be perfectly perpendicular
to the top and bottom edges. At this point, we save the binding deskew angle
for possible later use. We will discuss deskew angles at a later step.


<a name="find_binding_thresh">Find binding threshold.</a>
--------------------------------------------------------------------------------

Although we calculated an inital bitonalization threshold earlier, we can also
use the binding to give us another bitonalization threshold. We have found that
the luma value halfway between the dark pixels of the binding and the bright
pixels of the page works well for bitonalizaton, so we return this value from
`FindBindingEdge2()` for later use.
TODO: calculate this inititally and use it instead.



<a name="calculate_initial_cropbox">Calculate initial cropbox.</a>
--------------------------------------------------------------------------------

We now have initial crop lines for all four sides of the reduced image. We
scale this crop box by 8x to fit the fullsize image.


<a name="open_fullsize_image">Open fullsize image.</a>
--------------------------------------------------------------------------------

We now open the fullsize image for crop box refinement. Since it takes more than
one second to open the fullsize JPEG, this algorithm can't be used in realtime.


<a name="convert_to_gray_full">Convert fullsize image to grayscale.</a>
--------------------------------------------------------------------------------

We use the same single/three-channel technique as above to create a grayscale
image.


<a name="bintonalize_full">Bitonalize image using binding bitonalization threshold.</a>
--------------------------------------------------------------------------------

We have found that the binding bitonalization threshold works better than the
one we calculated initially, so we use the binding threshold to bitonalize the
fullsize image.


<a name="find_text_skew">Calculate text deskew angle.</a>
--------------------------------------------------------------------------------

In order to determine a good crop box, we must first deskew the image, which
we haven't yet done. In a previous step, we calculated the angle to deskew the
binding, but due to problems with printing and/or binding, the text may not
actually be perfectly perpendicular to the binding. Also, the page may be
warped, so that the text at the top of the page is printed at a different angle
than the bottom.

If we can, we want to deskew the image so that the text is straight, regardless
of the angle of the page edges. We reduce the cropbox by ten percent of the page
width/height on all sides (MAGIC_NUMBER), and pass the cropped bitonal image
to Leptonica's `pixFindSkew()` function, which returns a deskew angle and a
confidence.

Leptonica's `pixFindSkew()` finds a deskew angle that maximizes the sum of
squared differences (SSD) of two adjacent scanlines. The confidence it returns is
the ratio of the max SSD to the min SSD.


<a name="skew_mode">Determine skew mode and deskew angle.</a>
--------------------------------------------------------------------------------

As stated previously, we should deskew the text on the page, if possible. If
Leptonica's `pixFindSkew()` returns a confidence greater than 2.0 (MAGIC_NUMBER),
then we use the text skew angle to deskew the image. Otherwise, we use the
binding deskew angle.

<a name="deskew">Deskew fullsize rotated grayscale image.</a>
--------------------------------------------------------------------------------

We deskew the fullsize image about the center.


<a name="#refine_outer_edge">Refine outer edge calculation.</a>
--------------------------------------------------------------------------------

The initial crop line for the outer edge has been placed at the edge of the
back book cover. However, the leaf edges of all the pages behind the current
page also appear in the image, and we need to adjust the crop line to exclude
these leaf edges. This is done by `FindOuterEdgeUsingCleanLines()`.
TODO: document how this function works.


<a name="find_clean_lines">Find clean crop lines.</a>
--------------------------------------------------------------------------------

Crop box refinement is performed by the `RemoveBlackPelsBlockRow/Col` functions.

A kernel that is 80% of the length of the initial crop line and 3 pixels wide
(MAGIC_NUMBER) is moved in from the initial cropbox until it finds a block with
fewer than 5 black pixels (MAGIC_NUMBER).


<a name="write_output">Write output.</a>
--------------------------------------------------------------------------------

We write out the crop and deskew parameters for later processing.
