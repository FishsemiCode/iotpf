/****************************************************************************
 * external/services/iotpf/cis_utils.c
 *
 *     Copyright (C) 2019 FishSemi Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#include "cis_internals.h"
#include "cis_log.h"

#include <float.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>


int utils_checkBigendian(void)
{
  union
    {
      unsigned long int i;
      unsigned char s[4];
    }c;
  c.i = 0x12345678;
  return (0x12 == c.s[0]);
}



int utils_plainTextToInt64(uint8_t *buffer,
    int length, int64_t *dataP)
{
  uint64_t result = 0;
  int sign = 1;
  int i = 0;

  if (0 == length)
    {
      return 0;
    }
  if (buffer[0] == '-')
    {
      sign = -1;
      i = 1;
    }

  while (i < length)
    {
      if ('0' <= buffer[i] && buffer[i] <= '9')
        {
          if (result > (UINT64_MAX / 10))
            {
              return 0;
            }
          result *= 10;
          result += buffer[i] - '0';
        }
      else
        {
          return 0;
        }
      i++;
    }

  if (result > INT64_MAX)
    {
      return 0;
    }
  if (sign == -1)
    {
      *dataP = 0 - result;
    }
  else
    {
      *dataP = result;
    }
  return 1;
}

int utils_plainTextToFloat64(uint8_t *buffer,
                             int length,
                             double *dataP)
{
  double result;
  int sign;
  int i;

  if (0 == length)
    {
      return 0;
    }

  if (buffer[0] == '-')
    {
      sign = -1;
      i = 1;
    }
  else
    {
      sign = 1;
      i = 0;
    }

  result = 0;
  while (i < length && buffer[i] != '.')
    {
      if ('0' <= buffer[i] && buffer[i] <= '9')
        {
          if (result > (DBL_MAX / 10))
            {
              return 0;
            }
          result *= 10;
          result += (buffer[i] - '0');
        }
      else
        {
          return 0;
        }
      i++;
    }
  if (buffer[i] == '.')
    {
      double dec;
      i++;
      if (i == length)
        {
          return 0;
        }
      dec = 0.1;
      while (i < length)
        {
          if ('0' <= buffer[i] && buffer[i] <= '9')
            {
              if (result > (DBL_MAX - 1))
                {
                  return 0;
                }
              result += (buffer[i] - '0') * dec;
              dec /= 10;
            }
          else
            {
              return 0;
            }
          i++;
        }
    }
  *dataP = result * sign;
  return 1;
}

size_t cis_utils_intToText(int64_t data,
                       uint8_t *string,
                       size_t length)
{
  int index;
  bool minus;
  size_t result;

  if (data < 0)
    {
      minus = true;
      data = 0 - data;
    }
  else
    {
      minus = false;
    }

  index = length - 1;
  do
    {
      string[index] = '0' + data % 10;
      data /= 10;
      index --;
    } while (index >= 0 && data > 0);

  if (data > 0)
    {
      return 0;
    }

  if (minus == true)
    {
      if (index == 0)
        {
          return 0;
        }
      string[index] = '-';
    }
  else
    {
      index++;
    }

  result = length - index;

  if (result < length)
    {
      cis_memmove(string, string + index, result);
    }
  return result;
}

size_t utils_floatToText(double data,
                         uint8_t *string,
                         size_t length)
{
  size_t intLength;
  size_t decLength;
  int64_t intPart;
  double decPart;

  if (data <= (double)INT64_MIN || data >= (double)INT64_MAX)
    {
      string[0] = '\0';
      return 0;
    }

  intPart = (int64_t)data;
  decPart = data - intPart;
  if (decPart < 0)
    {
      decPart = 1 - decPart;
    }
  else
    {
      decPart = 1 + decPart;
    }

  if (decPart <= 1 + FLT_EPSILON)
    {
      decPart = 0;
    }

  if (intPart == 0 && data < 0)
    {
      if (length < 4)
        {
          string[0] = '\0';
          return 0;
        }
      string[0] = '-';
      string[1] = '0';
      intLength = 2;
    }
  else
    {
      intLength = cis_utils_intToText(intPart, string, length - 1);
      if (intLength == 0)
        {
          string[0] = '\0';
          return 0;
        }
    }
  decLength = 0;
  if (decPart >= FLT_EPSILON)
    {
      int i;
      double noiseFloor;
      if (intLength >= length - 1)
        {
          string[0] = '\0';
          return 0;
        }
      i = 0;
      noiseFloor = FLT_EPSILON;
      do
        {
          decPart *= 10;
          noiseFloor *= 10;
          i++;
      } while (decPart - (int64_t)decPart > noiseFloor);

      decLength = cis_utils_intToText((int64_t)decPart, string + intLength, length - intLength - 1);
      if (decLength <= 1)
        {
          string[0] = '\0';
          return 0;
        }
      string[intLength] = '.';
  }

  string[intLength + decLength] = '\0';
  return intLength + decLength;
}

size_t utils_int64ToPlainText(int64_t data,
                              uint8_t **bufferP)
{
#define _PRV_STR_LENGTH 32
  uint8_t string[_PRV_STR_LENGTH];
  size_t length;

  length = cis_utils_intToText(data, string, _PRV_STR_LENGTH);
  if (length == 0)
    {
      return 0;
    }

  *bufferP = (uint8_t *)cis_malloc(length);
  if (NULL == *bufferP)
    {
      return 0;
    }

  cis_memcpy(*bufferP, string, length);

  return length;
}


size_t utils_float64ToPlainText(double data,
                                uint8_t **bufferP)
{
  uint8_t string[_PRV_STR_LENGTH * 2];
  size_t length;

  snprintf((char *)string, (_PRV_STR_LENGTH * 2), "%.15lf", data);
  length = strlen((const char*)string);

  if (length == 0)
    {
      return 0;
    }

  *bufferP = (uint8_t *)cis_malloc(length);
  if (NULL == *bufferP)
    {
      return 0;
    }

  cis_memcpy(*bufferP, string, length);
  return length;
}


size_t utils_boolToPlainText(bool data,
                             uint8_t **bufferP)
{
  return utils_int64ToPlainText((int64_t)(data ? 1 : 0), bufferP);
}

et_binding_t cis_utils_stringToBinding(uint8_t *buffer,
                                      size_t length)
{
  if (length == 0)
    {
      return BINDING_UNKNOWN;
    }

  switch (buffer[0])
    {
      case 'U':
        {
          switch (length)
            {
              case 1:
                return BINDING_U;
              case 2:
                {
                  switch (buffer[1])
                    {
                      case 'Q':
                        return BINDING_UQ;
                      case 'S':
                        return BINDING_US;
                      default:
                        break;
                      }
                }
                break;
              case 3:
                {
                  if (buffer[1] == 'Q' && buffer[2] == 'S')
                    {
                      return BINDING_UQS;
                    }
                }
                break;
              default:
                break;
            }
        }
        break;
      case 'S':
        {
          switch (length)
            {
              case 1:
                return BINDING_S;
              case 2:
                if (buffer[1] == 'Q')
                  {
                    return BINDING_SQ;
                  }
                break;
              default:
                break;
            }
        }
        break;
      default:
        break;
    }
  return BINDING_UNKNOWN;
}

et_media_type_t utils_convertMediaType(coap_content_type_t type)
{
  switch((uint16_t)type)
    {
      case TEXT_PLAIN:
        return LWM2M_CONTENT_TEXT;
      case APPLICATION_OCTET_STREAM:
        return LWM2M_CONTENT_OPAQUE;
      case LWM2M_CONTENT_TLV_NEW:
        return LWM2M_CONTENT_TLV_NEW;
      case LWM2M_CONTENT_TLV:
        return LWM2M_CONTENT_TLV;
      case LWM2M_CONTENT_JSON:
        return LWM2M_CONTENT_JSON;
      case APPLICATION_LINK_FORMAT:
        return LWM2M_CONTENT_LINK;
      default:
        return LWM2M_CONTENT_TEXT;
    }
}


int utils_opaqueToInt(const uint8_t *buffer,
                      size_t buffer_len,
                      int64_t *dataP)
{
  *dataP = 0;

  switch (buffer_len)
    {
      case 1:
        {
          *dataP = (int8_t)buffer[0];
          break;
        }
      case 2:
        {
          int16_t value;
          cis_utils_copyValue(&value, buffer, buffer_len);
          *dataP = value;
          break;
        }
      case 4:
        {
          int32_t value;
          cis_utils_copyValue(&value, buffer, buffer_len);
          *dataP = value;
          break;
        }
      case 8:
        cis_utils_copyValue(dataP, buffer, buffer_len);
        return buffer_len;
      default:
        return 0;
    }
  return buffer_len;
}

int utils_opaqueToFloat(const uint8_t *buffer,
                        size_t buffer_len,
                        double *dataP)
{
  switch (buffer_len)
    {
      case 4:
        {
          float temp;
          cis_utils_copyValue(&temp, buffer, buffer_len);
          *dataP = temp;
        }
        return 4;
      case 8:
        cis_utils_copyValue(dataP, buffer, buffer_len);
        return 8;
      default:
        return 0;
    }
}

/**
* Encode an integer value to a byte representation.
* Returns the length of the result. For values < 0xff length is 1,
* for values < 0xffff length is 2 and so on.
* @param data        Input value
* @param data_buffer Result in data_buffer is in big endian encoding
*                    Negative values are represented in two's complement as of
*                    OMA-TS-LightweightM2M-V1_0-20160308-D, Appendix C
*/
size_t utils_encodeInt(int64_t data,
                       uint8_t data_buffer[_PRV_64BIT_BUFFER_SIZE])
{
  size_t length = 0;

  cis_memset(data_buffer, 0, _PRV_64BIT_BUFFER_SIZE);

  if (data >= INT8_MIN && data <= INT8_MAX)
    {
      length = 1;
      data_buffer[0] = (uint8_t)data;
    }
  else if (data >= INT16_MIN && data <= INT16_MAX)
    {
      int16_t value;
      value = (int16_t)data;
      length = 2;
      data_buffer[0] = (value >> 8) & 0xFF;
      data_buffer[1] = value & 0xFF;
    }
  else if (data >= INT32_MIN && data <= INT32_MAX)
    {
      int32_t value;
      value = (int32_t)data;
      length = 4;
      cis_utils_copyValue(data_buffer, &value, length);
    }
  else if (data >= INT64_MIN && data <= INT64_MAX)
    {
      length = 8;
      cis_utils_copyValue(data_buffer, &data, length);
    }
  return length;
}

size_t utils_encodeFloat(double data,
                         uint8_t data_buffer[_PRV_64BIT_BUFFER_SIZE])
{
  size_t length = 0;

  cis_memset(data_buffer, 0, _PRV_64BIT_BUFFER_SIZE);

  if ((data < 0.0 - (double)FLT_MAX) || (data >(double)FLT_MAX))
    {
      length = 8;
      cis_utils_copyValue(data_buffer, &data, 8);
    }
  else
    {
      double value;
      length = 8;
      value = (double)data;
      cis_utils_copyValue(data_buffer, &value, 8);
    }
  return length;
}


// copy a string in a buffer.
// return the number of copied bytes or -1 if the buffer is not large enough
int cis_utils_stringCopy(char *buffer,
    size_t length,
    const char *str)
{
  size_t i;

  for (i = 0; i < length && str[i] != 0; i++)
    {
      buffer[i] = str[i];
    }

  if (i == length)
    {
      return -1;
    }

  buffer[i] = 0;
  return (int)i;
}

int utils_intCopy(char *buffer,
    size_t length,
    int32_t value)
{
#define _PRV_INT32_MAX_STR_LEN 11
  uint8_t str[_PRV_INT32_MAX_STR_LEN];
  size_t len;

  len = cis_utils_intToText(value, str, _PRV_INT32_MAX_STR_LEN);
  if (len == 0)
    {
      return -1;
    }
  if (len > length + 1)
    {
      return -1;
    }
  cis_memcpy(buffer, str, len);
  buffer[len] = 0;
  return len;
}

void cis_utils_copyValue(void *dst,
    const void *src,
    size_t len)
{
  size_t i;

  if (utils_checkBigendian())
    {
      cis_memcpy(dst, src, len);
    }
  else
    {
      for (i = 0; i < len; i++)
        {
          ((uint8_t *)dst)[i] = ((uint8_t *)src)[len - 1 - i];
        }
    }
}


char *utils_strdup(const char *str)
{
  size_t length = utils_strlen(str);
  char *result = (char *)cis_malloc( length +1);
  if (result == NULL)
    {
      return NULL;
    }
  cis_memcpy(result, str, length);
  result[length] = 0;
  return result;
}

uint8_t *utils_bufferdup(const uint8_t *src,size_t len)
{
  uint8_t* buffer = NULL;
  buffer = (uint8_t*)cis_malloc(len);
  cissys_assert(buffer != NULL);
  if (buffer == NULL)
    {
      return NULL;
    }
  cis_memcpy(buffer, src, len);
  return buffer;
}

#if !CIS_ENABLE_MEMORYTRACE
void *cis_malloc(size_t s)
{
  uint8_t *p = (uint8_t*)cissys_malloc(s);
  if (p == NULL)
    {
      LOGE("ERROR:cis_malloc failed.\n");
    }
  return p;
}

void cis_free(void *p)
{
  if (p != NULL)
    {
      cissys_free(p);
    }
}

#endif//CIS_ENABLE_MEMORY_TRACE

int utils_strncasecmp(const char *s1,
                     const char *s2,
                     size_t n)
{
  int c1,c2,d;
  do
    {
      c1 = *s1++;
      c2 = *s2++;
      if (!n--)
        {
          return 0;
        }
      d = c1 - c2;
      while (d)
        {
          if (c1 <= 'Z' && c1 >= 'A')
            {
              d += ('a' - 'A');
              if (!d)
                {
                  break;
                }
            }
          if (c2 <= 'Z' && c2 >= 'A')
            {
              d -= ('a' - 'A');
              if (!d)
                {
                  break;
                }
            }
          return ((d >= 0) << 1) - 1;
        }
    } while (c1);
  return 0;
}

int utils_strncmp(const char *s1, const char *s2, size_t n)
{
  int c1,c2,d;
  do
    {
      c1 = *s1++;
      c2 = *s2++;
      if (!n--)
        {
          return 0;
        }
      d = c1 - c2;
      if (d)
        {
          return ((d >= 0) << 1) - 1;
        }
    } while (c1);
  return 0;
}

cis_time_t utils_gettime_s(void)
{
  return cissys_gettime() / 1000;
}

int utils_strlen(const char *str)
{
  int siz = 0;
  while (str && *str++ != '\0')
    {
      siz++;
    }
  return siz;
}

///////////////
//#if CIS_ENABLE_LOG
#define CIS_MAX_NUMBER_BUFFER   32
#define CIS_NUM_DEFAULT         (-1)

int utils_strtoi(const char *str, char **endptr)
{
  int neg = 1;
  const char *p = str;
  int value = 0;

  if (!str)
    {
      return 0;
    }

  /* skip whitespace */
  while (*p == ' ')
    {
      p++;
    }
  if (*p == '-')
    {
      neg = -1;
      p++;
    }
  while (*p && *p >= '0' && *p <= '9')
    {
      value = value * 10 + (int) (*p - '0');
      p++;
    }
  if (endptr)
    {
      *endptr = (char*)p;
    }
  return neg*value;
}


int utils_vsnprintf(char *buf, size_t buf_size, const char *fmt, va_list args)
{
  enum nbs_arg_type
    {
      NBS_ARG_TYPE_CHAR,
      NBS_ARG_TYPE_SHORT,
      NBS_ARG_TYPE_DEFAULT,
      NBS_ARG_TYPE_LONG
    };
  enum nbs_arg_flags
    {
      NBS_ARG_FLAG_LEFT = 0x01,
      NBS_ARG_FLAG_PLUS = 0x02,
      NBS_ARG_FLAG_SPACE = 0x04,
      NBS_ARG_FLAG_NUM = 0x10,
      NBS_ARG_FLAG_ZERO = 0x20
    };
  char number_buffer[CIS_MAX_NUMBER_BUFFER];
  enum nbs_arg_type arg_type = NBS_ARG_TYPE_DEFAULT;
  int precision = CIS_NUM_DEFAULT;
  int width = CIS_NUM_DEFAULT;
  uint32_t flag = 0;

  size_t len = 0;
  int result = -1;
  const char *iter = fmt;

  if (!buf || !buf_size || !fmt)
    {
      return 0;
    }

  for (iter = fmt; *iter && len < buf_size; iter++)
    {
      while (*iter && (*iter != '%') && (len < buf_size))
        {
          buf[len++] = *iter++;
        }
      if (!(*iter) || len >= buf_size)
        {
          break;
        }
      iter++;
      while (*iter)
        {
          if (*iter == '-')
            {
              flag |= NBS_ARG_FLAG_LEFT;
            }
          else if (*iter == '+')
            {
              flag |= NBS_ARG_FLAG_PLUS;
            }
          else if (*iter == ' ')
            {
              flag |= NBS_ARG_FLAG_SPACE;
            }
          else if (*iter == '#')
            {
              flag |= NBS_ARG_FLAG_NUM;
            }
          else if (*iter == '0')
            {
              flag |= NBS_ARG_FLAG_ZERO;
            }
          else
            {
              break;
            }
          iter++;
        }
      width = CIS_NUM_DEFAULT;
      if (*iter >= '1' && *iter <= '9')
        {
          char *end;
          width = utils_strtoi(iter, &end);
          if (end == iter)
            {
              width = -1;
            }
          else
            {
              iter = end;
            }
        }
      else if (*iter == '*')
        {
          width = va_arg(args, int);
          iter++;
        }
      precision = CIS_NUM_DEFAULT;
      if (*iter == '.')
        {
          iter++;
          if (*iter == '*')
            {
              precision = va_arg(args, int);
              iter++;
            }
          else
            {
              char *end;
              precision = utils_strtoi(iter, &end);
              if (end == iter)
                {
                  precision = -1;
                }
              else
                {
                  iter = end;
                }
            }
        }
      if (*iter == 'h')
        {
          if (*(iter+1) == 'h')
            {
              arg_type = NBS_ARG_TYPE_CHAR;
              iter++;
            }
          else
            {
              arg_type = NBS_ARG_TYPE_SHORT;
            }
          iter++;
        }
      else if (*iter == 'l')
        {
          arg_type = NBS_ARG_TYPE_LONG;
          iter++;
        }
      else
        {
          arg_type = NBS_ARG_TYPE_DEFAULT;
        }
      if (*iter == '%')
        {
          if (arg_type != NBS_ARG_TYPE_DEFAULT ||
            precision != CIS_NUM_DEFAULT ||
            width != CIS_NUM_DEFAULT)
            {
              return -1;
            }
          if (len < buf_size)
            {
              buf[len++] = '%';
            }
        }
      else if (*iter == 's')
        {
          const char *str = va_arg(args, const char*);
          if (arg_type != NBS_ARG_TYPE_DEFAULT ||
            width != CIS_NUM_DEFAULT)
            {
              return -1;
            }
          if (str == buf)
            {
              return -1;
            }
          if(precision != CIS_NUM_DEFAULT)
            {
              while (str && *str && len < buf_size && precision--)
                {
                  buf[len++] = *str++;
                }
            }
          else
            {
              while (str && *str && len < buf_size)
                {
                  buf[len++] = *str++;
                }
            }
        }
      else if (*iter == 'n')
        {
          signed int *n = va_arg(args, int*);
          if (arg_type != NBS_ARG_TYPE_DEFAULT ||
            precision != CIS_NUM_DEFAULT ||
            width != CIS_NUM_DEFAULT)
            {
              return -1;
            }
          if (n)
            {
              *n = len;
            }
        }
      else if (*iter == 'c')
        {
          long value = 0;
          const char *num_iter;
          value = va_arg(args, int);
          number_buffer[0] = (char)value;
          number_buffer[1] = '\0';
          num_iter = number_buffer;
          while (*num_iter && len < buf_size)
            {
              buf[len++] = *num_iter++;
            }
        }
      else if (*iter == 'i' || *iter == 'd')
        {
          long value = 0;
          const char *num_iter;
          int num_len, num_print, padding;
          int cur_precision = CIS_MAX(precision, 1);
          int cur_width = CIS_MAX(width, 0);
          if (arg_type == NBS_ARG_TYPE_CHAR)
            {
              value = (signed char)va_arg(args, int);
            }
          else if (arg_type == NBS_ARG_TYPE_SHORT)
            {
              value = (signed short)va_arg(args, int);
            }
          else if (arg_type == NBS_ARG_TYPE_LONG)
            {
              value = va_arg(args, signed long);
            }
          else
            {
              value = va_arg(args, signed int);
            }
          num_len = cis_utils_intToText( value, (uint8_t*)number_buffer, CIS_MAX_NUMBER_BUFFER-1);
          number_buffer[num_len] = '\0';
          padding = CIS_MAX(cur_width - CIS_MAX(cur_precision, num_len), 0);
          if ((flag & NBS_ARG_FLAG_PLUS) || (flag & NBS_ARG_FLAG_SPACE))
            {
              padding = CIS_MAX(padding-1, 0);
            }
          if (!(flag & NBS_ARG_FLAG_LEFT))
            {
              while (padding-- > 0 && (len < buf_size))
                {
                  if ((flag & NBS_ARG_FLAG_ZERO) && (precision == CIS_NUM_DEFAULT))
                    {
                      buf[len++] = '0';
                    }
                  else
                    {
                      buf[len++] = ' ';
                    }
                }
            }
          if ((flag & NBS_ARG_FLAG_PLUS) && value >= 0 && len < buf_size)
            {
              buf[len++] = '+';
            }
          else if ((flag & NBS_ARG_FLAG_SPACE) && value >= 0 && len < buf_size)
            {
              buf[len++] = ' ';
            }
          num_print = CIS_MAX(cur_precision, num_len);
          while (precision && (num_print > num_len) && (len < buf_size))
            {
              buf[len++] = '0';
              num_print--;
            }
          num_iter = number_buffer;
          while (precision && *num_iter && len < buf_size)
            {
              buf[len++] = *num_iter++;
            }
          if (flag & NBS_ARG_FLAG_LEFT)
            {
              while ((padding-- > 0) && (len < buf_size))
                {
                  buf[len++] = ' ';
                }
            }
        }
      else if (*iter == 'o' || *iter == 'x' || *iter == 'X' || *iter == 'u')
        {
          unsigned long value = 0;
          int num_len = 0, num_print, padding = 0;
          int cur_precision = CIS_MAX(precision, 1);
          int cur_width = CIS_MAX(width, 0);
          unsigned int base = (*iter == 'o') ? 8: (*iter == 'u')? 10: 16;
          const char *upper_output_format = "0123456789ABCDEF";
          const char *lower_output_format = "0123456789abcdef";
          const char *output_format = (*iter == 'x') ? lower_output_format: upper_output_format;
          if (arg_type == NBS_ARG_TYPE_CHAR)
            {
              value = (unsigned char)va_arg(args, int);
            }
          else if (arg_type == NBS_ARG_TYPE_SHORT)
            {
              value = (unsigned short)va_arg(args, int);
            }
          else if (arg_type == NBS_ARG_TYPE_LONG)
            {
              value = va_arg(args, unsigned long);
            }
          else
            {
              value = va_arg(args, unsigned int);
            }
          do
            {
              int digit = output_format[value % base];
              if (num_len < CIS_MAX_NUMBER_BUFFER)
                {
                  number_buffer[num_len++] = (char)digit;
                }
              value /= base;
            } while (value > 0);
          num_print = CIS_MAX(cur_precision, num_len);
          padding = CIS_MAX(cur_width - CIS_MAX(cur_precision, num_len), 0);
          if (flag & NBS_ARG_FLAG_NUM)
            {
              padding = CIS_MAX(padding-1, 0);
            }
          if (!(flag & NBS_ARG_FLAG_LEFT))
            {
              while ((padding-- > 0) && (len < buf_size))
                {
                  if ((flag & NBS_ARG_FLAG_ZERO) && (precision == CIS_NUM_DEFAULT))
                    {
                      buf[len++] = '0';
                    }
                  else
                    {
                      buf[len++] = ' ';
                    }
                }
            }
          if (num_print && (flag & NBS_ARG_FLAG_NUM))
            {
              if ((*iter == 'o') && (len < buf_size))
                {
                  buf[len++] = '0';
                }
              else if ((*iter == 'x') && ((len+1) < buf_size))
                {
                  buf[len++] = '0';
                  buf[len++] = 'x';
                }
              else if ((*iter == 'X') && ((len+1) < buf_size))
                {
                  buf[len++] = '0';
                  buf[len++] = 'X';
                }
            }
          while (precision && (num_print > num_len) && (len < buf_size))
            {
              buf[len++] = '0';
              num_print--;
            }
          while (num_len > 0)
            {
              if (precision && (len < buf_size))
                {
                  buf[len++] = number_buffer[num_len-1];
                }
              num_len--;
            }
          if (flag & NBS_ARG_FLAG_LEFT)
            {
              while ((padding-- > 0) && (len < buf_size))
                {
                  buf[len++] = ' ';
                }
            }
        }
      else if (*iter == 'f')
        {
          const char *num_iter;
          int cur_precision = (precision < 0) ? 6: precision;
          int prefix, cur_width = CIS_MAX(width, 0);
          double value = va_arg(args, double);
          int num_len = 0, frac_len = 0, dot = 0;
          int padding = 0;
          if (arg_type != NBS_ARG_TYPE_DEFAULT)
            {
              return -1;
            }
          utils_floatToText(value, (uint8_t*)number_buffer, CIS_MAX_NUMBER_BUFFER);
          num_len = utils_strlen(number_buffer);
          num_iter = number_buffer;
          while (*num_iter && *num_iter != '.')
            {
              num_iter++;
            }
          prefix = (*num_iter == '.') ? (int)(num_iter - number_buffer) + 1 : 0;
          padding = CIS_MAX(cur_width - (prefix + CIS_MIN(cur_precision, num_len - prefix)) , 0);
          if ((flag & NBS_ARG_FLAG_PLUS) || (flag & NBS_ARG_FLAG_SPACE))
            {
              padding = CIS_MAX(padding-1, 0);
            }
          if (!(flag & NBS_ARG_FLAG_LEFT))
            {
              while (padding-- > 0 && (len < buf_size))
                {
                  if (flag & NBS_ARG_FLAG_ZERO)
                    {
                      buf[len++] = '0';
                    }
                  else
                    {
                      buf[len++] = ' ';
                    }
                }
            }
          num_iter = number_buffer;
          if ((flag & NBS_ARG_FLAG_PLUS) && (value >= 0) && (len < buf_size))
            {
              buf[len++] = '+';
            }
          else if ((flag & NBS_ARG_FLAG_SPACE) && (value >= 0) && (len < buf_size))
            {
              buf[len++] = ' ';
            }
          while (*num_iter)
            {
              if (dot)
                {
                  frac_len++;
                }
              if (len < buf_size)
                {
                  buf[len++] = *num_iter;
                }
              if (*num_iter == '.')
                {
                  dot = 1;
                }
              if (frac_len >= cur_precision)
                {
                  break;
                }
              num_iter++;
            }
          while (frac_len < cur_precision)
            {
              if (!dot && len < buf_size)
                {
                  buf[len++] = '.';
                  dot = 1;
                }
              if (len < buf_size)
                {
                  buf[len++] = '0';
                }
              frac_len++;
            }
          if (flag & NBS_ARG_FLAG_LEFT)
            {
              while ((padding-- > 0) && (len < buf_size))
                {
                  buf[len++] = ' ';
                }
            }
        }
      else
        {
          return result;
        }
    }
  buf[(len >= buf_size)?(buf_size-1):len] = 0;
  result = (len >= buf_size)?-1:len;
  return result;
}

int utils_snprintf(char *buffer, size_t size, const char *format, ...)
{
  int result;
  va_list arguments;
  va_start(arguments, format);
  result = utils_vsnprintf(buffer, size, format, arguments);
  va_end(arguments);
  return result;
}

st_server_t *utils_findBootstrapServer(st_context_t *contextP,
    void *fromSessionH)
{
  st_server_t * targetP;
  targetP = contextP->bootstrapServer;
  while (targetP != NULL && fromSessionH != targetP->sessionH)
    {
      targetP = targetP->next;
    }
  return targetP;
}

void utils_hexStrToByte(const char *source, int sourceLen, unsigned char *dest)
{
  short i;
  unsigned char highByte, lowByte;

  for (i = 0; i < sourceLen; i += 2)
    {
      highByte = toupper(source[i]);
      lowByte  = toupper(source[i + 1]);
      if (highByte > 0x39)
        {
          highByte -= 0x37;
        }
      else
        {
          highByte -= 0x30;
        }
      if (lowByte > 0x39)
        {
          lowByte -= 0x37;
        }
      else
        {
          lowByte -= 0x30;
        }
      dest[i / 2] = (highByte << 4) | lowByte;
    }
  return;
}
