
types = ['u8', 's8', 'u16', 's16', 'float']

#should convert to and from uint16
readwriters = """


#define READu8(a)  ((a) << 8)
#define READs8(a)  ((a+128) << 8)
#define READu16(a)  (a)
#define READs16(a) ((a)+32768)
#define READfloat(a) ((a)*65536)

#define WRITEu8(a)  ((a) >> 8)
#define WRITEs8(a)  (((a) - 32768) >> 8)
#define WRITEu16(a) (a)
#define WRITEs16(a) ((a) - 32768)
#define WRITEfloat(a) ((a)/65536.0)



"""


resamplingcode = """
static guint
resample_INCHANNELS_INTYPE_to_OUTCHANNELS_OUTTYPE (xmms_sample_converter_t *conv, xmms_sample_t *tbuf, guint len, xmms_sample_t *tout)
{
	xmms_sampleINTYPE_t *buf = (xmms_sampleINTYPE_t *) tbuf;
	xmms_sampleOUTTYPE_t *out = (xmms_sampleOUTTYPE_t *) tout;
	xmms_sampleINTYPE_t *prevbuf;
	gfloat incr, pos;
       	gfloat npos;
	gint i, n=0;

	incr = conv->incr;
	
	pos = conv->offset;
	if (pos < 1) {
		prevbuf = (xmms_sampleINTYPE_t *)conv->state;
	} else {
		g_assert(0);
		prevbuf = &buf[INCHANNELS * (((gint)pos)-1)];
	}

	g_assert(pos >= 0);

	buf = &buf[INCHANNELS * (((gint)pos))];

	for (;;) {
		gint32 temp[INCHANNELS];
		gfloat bfrac = pos - floor(pos);
		gfloat afrac = 1.0 - bfrac;

		/* resample */
		for (i = 0; i < INCHANNELS; i++) {
			temp[i] = afrac * READINTYPE (prevbuf[i]) + bfrac * READINTYPE (buf[i]);
		}

		/* convert #channels into out[] */
CONVERTER

		n++;	
		npos = pos + incr;
		
		if (npos > len)
			break;

		out = &out[OUTCHANNELS];

		if (floor (npos) != floor (pos)) {
			gint idiff = (gint) (floor (npos) - floor (pos));
			
			if (idiff == 1) {
				prevbuf = buf;
				buf = &buf[INCHANNELS];
			} else {
				prevbuf = &buf[INCHANNELS * (idiff - 1)];
				buf = &buf[INCHANNELS * idiff];
			}
		}

		pos = npos;
	}

	conv->offset = npos - len;

	for (i = 0; i < INCHANNELS; i++) {
		((xmms_sampleINTYPE_t *)conv->state)[i] = buf[i];
	}
	return n;
}

static guint
convert_INCHANNELS_INTYPE_to_OUTCHANNELS_OUTTYPE (xmms_sample_converter_t *conv, void *tin, guint len, void *tout)
{
	xmms_sampleINTYPE_t *in = (xmms_sampleINTYPE_t *)tin;
	xmms_sampleOUTTYPE_t *out = (xmms_sampleOUTTYPE_t *)tout;
	gint i, j;
	
	/* this code doesn't look optimized,
	but the compiler should fix that */
	for (i = 0; i < len; i++) {
		gint32 temp[INCHANNELS];

		for (j = 0; j < INCHANNELS; j++) {
			temp[j] = READINTYPE (in[j]);
		}

		/* convert #channels into out[] */
CONVERTER
		out = &out[OUTCHANNELS];
		in = &in[INCHANNELS];
	}
	return len;
}



"""

import math

def cutoff(a):
	if a < 0.001:
		return 0.0
	return a

def Xget_channelconv(numin, numout):
	code = ""
	if numin < numout:
		incr = float (numin-1)/(numout-1)
		for i in range(numout):
			code += "out[%d] = " % i
			pos = i * incr
			offset = pos - math.floor (pos)
			code += "%f * temp[%d] + %f * temp[%d] //%f\n" % (
				1.0 - offset, int(math.floor (pos)),
				offset, int(math.ceil (pos)), pos)


	elif numin == numout:
		for i in range(numout):
			code += "out[%d] = temp[%d]\n" % (i,i)
	else: #numout < numin
		for i in range(numout):
			cent = (1.0 * (numin-1) * i / numout + 1.0 * (numin-1) * (i+1) / numout)/2
			gauss = [math.exp(-(numout*(float(a)-cent)/1.5)**2) for a in range(numin)]
			gauss = map(cutoff, gauss)
			s = sum(gauss)
			gauss = [a/s for a in gauss]
			parts=[]
			for j in range(numin):
				if gauss[j] > 0.0:
					parts.append("%f * temp[%d]" % (gauss[j], j))
			code += "out[%d] = %s\n" % (i, " + ".join(parts))

	return code


###
## This should really be expanded to handle
## other channel configurations!
##
def get_channelconv(numin, numout, t):
	out = ""
	if numin == numout:
		for a in range(numout):
			out += "\t\tout[%d] = WRITE%s(temp[%d]);\n" % (a,t,a)
	elif numin == 1 and numout == 2:
       		out += "\t\tout[0] = WRITE%s(temp[0]);\n" % t
       		out += "\t\tout[1] = WRITE%s(temp[0]);\n" % t
	elif numin == 2 and numout == 1:
       		out += "\t\tout[0] = WRITE%s((temp[0] + temp[1])/2);\n" % t
	else:
		raise RuntimeError("go implement channelconversion from %d to %d channels" % (numin, numout))
	return out

#print get_channelconv(3, 5)
#print get_channelconv(2, 5)
#print get_channelconv(1, 2)
#print get_channelconv(2, 2)

#print get_channelconv(5, 4)
#print get_channelconv(5, 2)
#print get_channelconv(5, 1)
#print get_channelconv(2, 1)


#print get_channelconv(4, 2)

#sys.exit(0)

#vars={}

import re

data = {'INCHANNELS' : range(1,3),
	'OUTCHANNELS' : range(1,3),
	'INTYPE' : types,
	'OUTTYPE' : types
	}

def make_conv(fields, curr):
	indent = "\t" * (len(curr)+1)
	if len(fields) == 0:
		#if curr['INCHANNELS'] == curr['OUTCHANNELS'] and curr['INTYPE'] == curr['OUTTYPE']:
		#	return ""

		out=resamplingcode
		for key,val in curr.iteritems():
			out = re.sub(key,str(val),out)

		out = re.sub("CONVERTER",
			     get_channelconv(curr['INCHANNELS'],
					     curr['OUTCHANNELS'],
					     curr['OUTTYPE']),
			     out)
		
		return out


	val = ""
	for a in data[fields[0]]:
		t = curr.copy()
		t[fields[0]] = a
		val += make_conv(fields[1:], t)
	return val


def make_switch(fields, curr):
	indent = "\t" * (len(curr)+1)
	if len(fields) == 0:
		#if curr['INCHANNELS'] == curr['OUTCHANNELS'] and curr['INTYPE'] == curr['OUTTYPE']:
		#	return indent + "return NULL;\n"
		suffix = "_%s_%s_to_%s_%s" % (
			curr['INCHANNELS'],
			curr['INTYPE'],
			curr['OUTCHANNELS'],
			curr['OUTTYPE'])
		return indent + "return resample ? resample%s : convert%s;\n" % (suffix, suffix)
		#return indent + "return convert%s;\n" % suffix

	val = indent + "switch(%s){\n" % fields[0].lower()
	val += indent + "default: return NULL;\n"
	for a in data[fields[0]]:
		t = curr.copy()
		t[fields[0]] = a
		if fields[0].endswith("TYPE"):
			val += indent + "case XMMS_SAMPLE_FORMAT_%s:\n" % a.upper()
		else:
			val += indent + "case %s:\n" % a
       		val += make_switch(fields[1:], t)
	val += indent + "}\n"
	return val

print readwriters
print make_conv(data.keys(),{})

print "static xmms_sample_conv_func_t"
print "xmms_sample_conv_get (guint inchannels, xmms_sample_format_t intype,"
print "                      guint outchannels, xmms_sample_format_t outtype,"
print "                      gboolean resample)"
print "{"
print make_switch(data.keys(),{})
print "\treturn NULL;"
print "}"

