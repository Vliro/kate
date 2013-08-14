/*  This file is part of the KDE libraries and the Kate part.
 *
 *  Copyright (C) 2008-2009 Erlend Hamberg <ehamberg@gmail.com>
 *  Copyright (C) 2013 Simon St James <kdedevel@etotheipiplusone.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */
#include "katevikeymapper.h"
#include "kateglobal.h"
#include "kateviglobal.h"

#include <QtCore/QTimer>

KateViKeyMapper::KateViKeyMapper(KateViInputModeManager* kateViInputModeManager, KateDocument* doc )
    : m_viInputModeManager(kateViInputModeManager),
      m_doc(doc)
{
  m_mappingKeyPress = false; // temporarily set to true when an aborted mapping sends key presses
  m_mappingTimer = new QTimer( this );
  m_doNotExpandFurtherMappings = false;
  m_timeoutlen = 1000; // FIXME: make configurable
  m_doNotMapNextKeypress = false;
  m_numMappingsBeingExecuted = 0;
  m_isPlayingBackRejectedKeys = false;
  m_doNotMapKeypressesCountDown = -1;
  connect(m_mappingTimer, SIGNAL(timeout()), this, SLOT(mappingTimerTimeOut()));
}

void KateViKeyMapper::executeMapping()
{
  m_mappingKeys.clear();
  m_mappingTimer->stop();
  m_numMappingsBeingExecuted++;
  const QString mappedKeypresses = KateGlobal::self()->viInputModeGlobal()->getMapping(m_viInputModeManager->getCurrentViMode(), m_fullMappingMatch);
  if (!KateGlobal::self()->viInputModeGlobal()->isMappingRecursive(m_viInputModeManager->getCurrentViMode(), m_fullMappingMatch))
  {
    kDebug(13070) << "Non-recursive: " << mappedKeypresses;
    m_doNotExpandFurtherMappings = true;
  }
  m_doc->editBegin();
  m_viInputModeManager->feedKeyPresses(mappedKeypresses);
  m_doNotExpandFurtherMappings = false;
  m_doc->editEnd();
  m_numMappingsBeingExecuted--;
}

void KateViKeyMapper::setMappingTimeout(int timeoutMS)
{
  m_timeoutlen = timeoutMS;
}

void KateViKeyMapper::mappingTimerTimeOut()
{
  kDebug( 13070 ) << "timeout! key presses: " << m_mappingKeys;
  m_mappingKeyPress = true;
  if (!m_fullMappingMatch.isNull())
  {
    executeMapping();
  }
  else
  {
    m_viInputModeManager->feedKeyPresses( m_mappingKeys );
  }
  m_mappingKeyPress = false;
  m_mappingKeys.clear();
}

bool KateViKeyMapper::handleKeypress(QChar key)
{
  if (m_doNotMapKeypressesCountDown > 0)
  {
    m_doNotMapKeypressesCountDown--;
    return false;
  }
  if ( !m_doNotExpandFurtherMappings && !m_mappingKeyPress && !m_doNotMapNextKeypress) {
    m_mappingKeys.append( key );

    bool isPartialMapping = false;
    bool isFullMapping = false;
    m_fullMappingMatch.clear();
    foreach ( const QString &mapping, KateGlobal::self()->viInputModeGlobal()->getMappings(m_viInputModeManager->getCurrentViMode()) ) {
      if ( mapping.startsWith( m_mappingKeys ) ) {
        if ( mapping == m_mappingKeys ) {
          isFullMapping = true;
          m_fullMappingMatch = mapping;
        } else {
          isPartialMapping = true;
        }
      }
    }
    if (isFullMapping && !isPartialMapping)
    {
      // Great - m_mappingKeys is a mapping, and one that can't be extended to
      // a longer one - execute it immediately.
      executeMapping();
      return true;
    }
    if (isPartialMapping)
    {
      // Need to wait for more characters (or a timeout) before we decide what to
      // do with this.
      m_mappingTimer->start( m_timeoutlen );
      m_mappingTimer->setSingleShot( true );
      return true;
    }
    // We've been swallowing all the keypresses meant for m_keys for our mapping keys; now that we know
    // this cannot be a mapping, restore them.
    Q_ASSERT(!isPartialMapping && !isFullMapping);
    // The keypresses we replay will of course go back through KateViKeyMapper, so we need to set a flag that
    // says "ignore (i.e. do not map) these keys".
    // However, we can't just set e.g. m_doNotExpandFurtherMappings to true and then back to false on either
    // side of feeding keypresses, as these keypresses may trigger a macro, which in turn will generate
    // keypresses that may form a mapping that we need to expand.
    // So instead, just say "ignore the next m_mappingKeys.length()" keypresses.
    // Note that this is not technically enough: if there is a mapping from "@aaaaaaaaaaaa" to something,
    // and we do "@aaab", and the macro in "a" contains keypresses that should be expanded into a mapping,
    // then these keypresses *won't* be mapped, I don't think.  This is a horribly pathological example, though, so
    // I'll ignore it for now.
    m_isPlayingBackRejectedKeys = true;
    m_doNotMapKeypressesCountDown = m_mappingKeys.length();
    const QString mappingKeys = m_mappingKeys;
    m_mappingKeys.clear();
    m_viInputModeManager->feedKeyPresses(mappingKeys);
    m_isPlayingBackRejectedKeys = false;
    return true;
  } else {
    // FIXME:
    //m_mappingKeyPress = false; // key press ignored wrt mappings, re-set m_mappingKeyPress
  }
  m_doNotMapNextKeypress = false;
  return false;
}

void KateViKeyMapper::setDoNotMapNextKeypress()
{
  m_doNotMapNextKeypress = true;
}

bool KateViKeyMapper::isExecutingMapping()
{
  return m_numMappingsBeingExecuted > 0;
}

bool KateViKeyMapper::isPlayingBackRejectedKeys()
{
  return m_isPlayingBackRejectedKeys;
}