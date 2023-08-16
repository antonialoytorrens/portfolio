import argparse
import os
from PIL import Image

def crop_and_resize(image_path, output_path, crop_width, crop_height):
    """
    Convert the input image to JPEG, rescale it to make one of the dimensions 
    equal to the desired size, maintaining the aspect ratio, and then crop 
    the image at the center to have the dimensions of crop_width x crop_height, 
    and save it to output_path.
    
    :param image_path: The path to the input image
    :param output_path: The path to save the cropped output image (in .jpg format)
    :param crop_width: The width of the cropped area
    :param crop_height: The height of the cropped area
    """
    
    # Open an image file
    with Image.open(image_path) as img:
        # Ensure the image is in RGB mode (required for saving as JPEG)
        img = img.convert('RGB')
        
        # Calculate aspect ratio
        aspect_ratio = img.width / img.height
        new_width = crop_width
        new_height = int(new_width / aspect_ratio)

        # Determine the new size that maintains aspect ratio
        if new_height < crop_height:
            new_height = crop_height
            new_width = int(new_height * aspect_ratio)

        # Resize the image
        img_rescaled = img.resize((new_width, new_height), Image.LANCZOS)
        
        # Calculate dimensions to crop the image in the center
        left = (new_width - crop_width) / 2
        top = (new_height - crop_height) / 2
        right = (new_width + crop_width) / 2
        bottom = (new_height + crop_height) / 2

        # Crop the image
        img_cropped = img_rescaled.crop((left, top, right, bottom))
        
        # Remove original extension and add .jpg
        output_path_base, _ = os.path.splitext(output_path)
        output_path_jpg = output_path_base + '.jpg'
        
        # Save the cropped image to output_path in JPEG format
        img_cropped.save(output_path_jpg, 'JPEG')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Convert, rescale, and crop an image to a specified size from the center.')
    parser.add_argument('origin_image', help='The path to the input image')
    parser.add_argument('destination_image', help='The path to save the cropped output image (in .jpg format)')
    parser.add_argument('--crop_width', type=int, default=440, help='The width of the cropped area (default: 440)')
    parser.add_argument('--crop_height', type=int, default=440, help='The height of the cropped area (default: 440)')
    
    args = parser.parse_args()

    crop_and_resize(args.origin_image, args.destination_image, args.crop_width, args.crop_height)