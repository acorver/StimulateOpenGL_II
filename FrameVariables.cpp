#include "FrameVariables.h"
#include <stdarg.h>
#include <QByteArray>
#include <QTextStream>
#include <QDate>

QString FrameVariables::lastFileName;

FrameVariables::FrameVariables(const QString &of, const QStringList & varnames)
: f(of), cnt(0), fname(of)
{
	lastFileName = fname;
	setVariableNames(varnames);
}

FrameVariables::~FrameVariables()
{
	finalize();
}

void FrameVariables::setVariableNames(const QStringList &fields)
{
	if (count()) {
		Error() << "setVariableNames() called with a non-empty FrameVariables instance!  reset() it before calling setVariableNames!";
		return;
	}
	n_fields = fields.count();
	var_names = fields;

}
void FrameVariables::push(double varval0...) ///< all vars must be doubles, and they must be the same number of parameters as the variableNames() list length!
{
	if (!n_fields) {
		Error() << "INTERNAL ERROR: FrameVariables::push() called with no variable names (column names) specified!  Did you forget to call FrameVars::setVariableNames?";
		return;
	}

	if (!f.isOpen()) {
		if (!f.open(QIODevice::WriteOnly|QIODevice::Text|QIODevice::Truncate)) {
			Error() << "Could not open frame variable output file: " << fname;
		} else {
			Log() << "Opened frame variables save file: " << fname;
		}
		ts.setDevice(&f);
	}

	va_list ap;
	va_start(ap, varval0);

	if (!cnt) { // write header..
		for (unsigned i = 0; i < n_fields; ++i) {
			if (i) ts << " ";
			ts << "\"" << var_names[i] << "\"";
		}
		ts << "\n";
	}
	ts << varval0;
	for (unsigned i = 1; i < n_fields; ++i) {
		ts << " ";
		ts << va_arg(ap, double);
	}
	ts << "\n";
	va_end(ap);
	++cnt;
}


bool FrameVariables::readAllFromFile(QVector<double> & out, int * nrows_out, int * ncols_out, bool matlab) const
{
	if (f.isOpen()) {
		ts.flush();
		f.flush();
	}
	return readAllFromFile(fileName(), out, nrows_out, ncols_out, matlab);
}

/* static */
bool FrameVariables::readAllFromFile(const QString &fn, QVector<double> & out, int * nrows, int * ncols, bool matlab) 
{
	out.clear();
	out.reserve(4096);
	if (nrows) *nrows = 0;
	if (ncols) *ncols = 0;
	QFile fin(fn);
	if (fin.open(QIODevice::ReadOnly)) {
		QTextStream ts (&fin);
		// skip header
		QStringList hdrLst = splitHeader(ts.readLine());
		const int nfields = hdrLst.count();
		double tmp;
		unsigned long capleft = out.capacity();
		while (!ts.atEnd()) {
			ts >> tmp;
			if (ts.status() == QTextStream::Ok) {
				if (!capleft) out.reserve(capleft=(out.capacity()*2));
				out.push_back(tmp);
				--capleft;
			}
		}
		const int numrows = out.size()/nfields;
		if (nrows) *nrows = numrows;
		if (ncols) *ncols = nfields;
		out.squeeze();
		if (matlab) { /// xform the matrix to matlab format..
			QVector<double> out2(out);
			for (int i = 0; i < numrows; ++i) {
				for (int j = 0; j < nfields; ++j) {
					// swap col for row
					double & v1 = out2[i*nfields+j];
					double & v2 = out[j*numrows+i];
					v2 = v1;
				}
			}			
		}
		return true;
	}
	return false;
}





/* static */ QString FrameVariables::makeFileName(const QString & prefix)
{
	const QDate now (QDate::currentDate());

	QString dateStr;
	dateStr.sprintf("%04d%02d%02d",now.year(),now.month(),now.day());

	QString fn = "";
	for(int i = 1; QFile::exists(fn = (prefix + "_" + dateStr + "_" + QString::number(i) + ".txt")); ++i)
		;
	return fn;
}

void FrameVariables::finalize()
{
	ts.flush();
	f.close();
}

/* static */ QStringList FrameVariables::splitHeader(const QString & ln) 
{
		QStringList lst = ln.trimmed().split(QRegExp("\\\" \\\""), QString::SkipEmptyParts);
		if (lst.count()) {
			if (lst.first().startsWith("\"")) lst.replace(0, lst.first().right(lst.first().length()-1));
			QString l;
			if ((l=lst.last()).endsWith("\"")) {
				lst.removeLast();
			    l = l.left(l.length()-1);
				lst.push_back(l);
			}
		}
		return lst;
}

QStringList FrameVariables::readHeaderFromLast()
{
	QFile fin(lastFileName);
	if (fin.open(QIODevice::ReadOnly)) 
		return splitHeader(fin.readLine());
	return QStringList();
}