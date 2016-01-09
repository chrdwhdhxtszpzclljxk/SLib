#include "../../../inc/slib/graphics/image.h"

#ifdef SLIB_GRAPHICS_IMAGE_SUPPORT_PNG

#include "../../../inc/slib/core/file.h"

#include "../../../inc/thirdparty/libpng/png.h"
#include "../../../inc/thirdparty/libpng/pngstruct.h"

SLIB_GRAPHICS_NAMESPACE_BEGIN
Ref<Image> Image::loadFromPNG(const void* content, sl_size size)
{
	png_image image;
	Base::resetMemory(&image, 0, sizeof(image));
	image.version = PNG_IMAGE_VERSION;

	Ref<Image> ret;
	if (png_image_begin_read_from_memory(&image, content, size)) {

		image.format = PNG_FORMAT_RGBA;
		sl_uint32 pitch = image.width * 4;
		sl_uint32 size = pitch * image.height;

		if (size > 0) {
			Memory mem = Memory::create(size);
			if (mem.isNotEmpty()) {
				png_bytep buffer = (png_bytep)(mem.getBuf());
				if (png_image_finish_read(&image, NULL, buffer, (png_int_32)pitch, NULL)) {
					ret = Image::createStatic(image.width, image.height, (Color*)buffer, image.width, mem.getObject());
				}
			}
		}

		png_image_free(&image);
	}

	return ret;
}

static void _slib_image_png_mem_write_callback(png_structp png_ptr, png_bytep data, png_size_t length)
{
	if (png_ptr == NULL)
		return;
	MemoryWriter& writer = *((MemoryWriter*)(png_ptr->io_ptr));
	writer.write(data, length);
}

static void _slib_image_png_encode_warning(png_structrp png_ptr, png_const_charp warning_message)
{
}

Memory Image::saveToPNG(const Ref<Image>& image)
{
	if (image.isNull()) {
		return Memory::null();
	}

	/* initialize stuff */
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	Memory ret;

	if (png_ptr) {

		png_ptr->warning_fn = _slib_image_png_encode_warning;

		MemoryWriter writer;

		if (!setjmp(png_jmpbuf(png_ptr))) {

			png_infop info_ptr = png_create_info_struct(png_ptr);
			if (info_ptr) {

				png_set_write_fn(png_ptr, &writer, _slib_image_png_mem_write_callback, NULL);

				sl_uint32 width = image->getWidth();
				sl_uint32 height = image->getHeight();
				sl_uint32 stride = image->getStride();
				const Color* pixels = image->getColors();
				png_set_IHDR(png_ptr, info_ptr
					, (png_uint_32)width, (png_uint_32)height
					, 8, PNG_COLOR_TYPE_RGB_ALPHA
					, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

				png_write_info(png_ptr, info_ptr);

				for (sl_uint32 i = 0; i < height; i++) {
					png_write_row(png_ptr, (png_const_bytep)pixels);
					pixels += stride;
				}

				png_write_end(png_ptr, NULL);

				ret = writer.getData();

				png_destroy_info_struct(png_ptr, &info_ptr);
			}
		}
		png_destroy_write_struct(&png_ptr, NULL);
	}

	return ret;
}

sl_bool Image::saveToPNG(String filePath, const Ref<Image>& image)
{
	if (image.isNull()) {
		return sl_false;
	}
	Ref<File> file = File::openForWrite(filePath);
	if (file.isNotNull()) {
		Memory mem = saveToPNG(image);
		if (mem.isNotEmpty()) {
			if (file->write(mem.getBuf(), mem.getSize()) == mem.getSize()) {
				return sl_true;
			}
		}
		file->close();
		File::deleteFile(filePath);
	}
	return sl_false;
}

SLIB_GRAPHICS_NAMESPACE_END

#endif
