#include <qlistview.h>

#include <xmms/xmmsclient.h>
#include <xmms/xmmsclient-qt.h>
#include "xmmslistview.h"

XMMSListViewItem::XMMSListViewItem (XMMSListView *parent, 
				    unsigned int id, QListViewItem *after) :
		  QListViewItem (parent, after)
{
	m_id = id;
	m_current = FALSE;
}

void
XMMSListViewItem::setArtist (const QString &artist)
{
	m_artist = artist.copy ();
}

void
XMMSListViewItem::setAlbum (const QString &album)
{
	m_album = album.copy ();
}

void
XMMSListViewItem::setTitle (const QString &title)
{
	m_title = title.copy ();
}

void
XMMSListViewItem::setDuration (int duration)
{
	m_duration = duration;
}

void
XMMSListViewItem::setURL (const QString &url)
{
	m_url = url.copy ();
}

void
XMMSListViewItem::setCurrent (bool b)
{
	m_current = b;
	repaint ();
}

void
XMMSListViewItem::paintCell (QPainter *p, const QColorGroup &cg, int c, int w, int a)
{

	if (!m_current)
		QListViewItem::paintCell (p, cg, c, w, a);
	else {
		QColorGroup ncg (cg);
		QColor oc = ncg.text ();

		ncg.setColor (QColorGroup::Text, Qt::red);
		ncg.setColor (QColorGroup::HighlightedText, Qt::red);

		QListViewItem::paintCell (p, ncg, c, w, a);
		
		ncg.setColor (QColorGroup::Text, oc);
		ncg.setColor (QColorGroup::HighlightedText, oc);
	}

}

QString
XMMSListViewItem::text (int pos) const
{

	switch (pos) {
		case 0:
			return QString::number (m_id);
		case 1:
			if (m_artist)
				return m_artist;

			return m_url.right (m_url.length () - m_url.findRev ('/') - 1);
		case 2:
			return m_album ? m_album : "";
		case 3:
			return m_title ? m_title : "";
		case 4:
			{
				char s[10];
				snprintf (s, 10, "%02d:%02d",
						m_duration/60000,
						(m_duration/1000)%60);

				return QString (s);
			}
	}
}

XMMSListView::XMMSListView (XMMSClientQT *client, QWidget *parent, const char *name) :
	      QListView (parent, name)
{

	addColumn ("ID", 42);
	addColumn ("Artist", 200);
	addColumn ("Album", 200);
	addColumn ("Titel", 200);
	addColumn ("Duration", 60);
	setSorting (-1, TRUE);
	setSelectionMode (QListView::Extended);
	setAllColumnsShowFocus (TRUE);
	setTreeStepSize (0);
	setRootIsDecorated (TRUE);

	m_client = client;

	connect (this, SIGNAL (doubleClicked (QListViewItem *, const QPoint &, int)),
		 this, SLOT (onDoubleClick (QListViewItem *, const QPoint &, int)));

}

void
XMMSListView::onDoubleClick (QListViewItem *i, const QPoint &, int)
{
	XMMSListViewItem *it = (XMMSListViewItem *)i;

	if (it) {
		xmmsc_playback_stop (m_client->getConnection ());
		xmmsc_playlist_jump (m_client->getConnection (), it->Id ());
		xmmsc_playback_start (m_client->getConnection ());
	}
	
}

