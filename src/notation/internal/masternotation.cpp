/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore BVBA and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "masternotation.h"

#include <QFileInfo>
#include "translation.h"

#include "excerptnotation.h"
#include "masternotationparts.h"
#include "scoreorderconverter.h"
#include "excerptnotation.h"
#include "../notationerrors.h"

#include "libmscore/masterscore.h"
#include "libmscore/part.h"
#include "libmscore/staff.h"
#include "libmscore/excerpt.h"
#include "libmscore/measure.h"
#include "libmscore/box.h"
#include "libmscore/keysig.h"
#include "libmscore/rest.h"
#include "libmscore/tempotext.h"
#include "libmscore/undo.h"

#include "log.h"

using namespace mu::notation;
using namespace mu::async;

static ExcerptNotation* get_impl(const IExcerptNotationPtr& excerpt)
{
    return static_cast<ExcerptNotation*>(excerpt.get());
}

MasterNotation::MasterNotation()
    : Notation()
{
    m_parts = std::make_shared<MasterNotationParts>(this, interaction(), undoStack());

    m_parts->partsChanged().onNotify(this, [this]() {
        notifyAboutNotationChanged();
    });
}

MasterNotation::~MasterNotation()
{
    m_parts = nullptr;
}

INotationPtr MasterNotation::notation()
{
    return shared_from_this();
}

void MasterNotation::setMasterScore(Ms::MasterScore* score)
{
    if (masterScore() == score) {
        return;
    }

    setScore(score);

    initExcerpts(masterScore()->excerpts());
}

Ms::MasterScore* MasterNotation::masterScore() const
{
    return dynamic_cast<Ms::MasterScore*>(score());
}

//! NOTE: this method with all of its dependencies was copied from MU3
//! source: file.cpp, MuseScore::getNewFile()
mu::Ret MasterNotation::setupNewScore(Ms::MasterScore* score, Ms::MasterScore* templateScore, const ScoreCreateOptions& scoreOptions)
{
    int measures = scoreOptions.measures;

    Ms::KeySigEvent ks;
    ks.setKey(scoreOptions.key);
    Ms::VBox* nvb = nullptr;

    bool pickupMeasure = scoreOptions.withPickupMeasure;
    if (pickupMeasure) {
        measures += 1;
    }

    QList<Ms::Excerpt*> excerpts;
    if (templateScore) {
        score->setStyle(templateScore->style());
        score->setScoreOrder(templateScore->scoreOrder());

        // create instruments from template
        for (Ms::Part* tpart : templateScore->parts()) {
            Ms::Part* part = new Ms::Part(score);
            part->setInstrument(tpart->instrument());
            part->setPartName(tpart->partName());

            for (Ms::Staff* tstaff : *tpart->staves()) {
                Ms::Staff* staff = Ms::createStaff(score, part);
                staff->init(tstaff);
                if (tstaff->links() && !part->staves()->isEmpty()) {
                    Staff* linkedStaff = part->staves()->back();
                    staff->linkTo(linkedStaff);
                }
                score->appendStaff(staff);
            }
            score->appendPart(part);
        }
        for (Ms::Excerpt* ex : templateScore->excerpts()) {
            Ms::Excerpt* x = new Ms::Excerpt(score);
            x->setTitle(ex->title());
            for (Part* p : ex->parts()) {
                int pidx = templateScore->parts().indexOf(p);
                if (pidx == -1) {
                    LOGE() << "part not found";
                } else {
                    x->parts().append(score->parts()[pidx]);
                }
            }
            excerpts.append(x);
        }
        Ms::MeasureBase* mb = templateScore->first();
        if (mb && mb->isVBox()) {
            Ms::VBox* tvb = toVBox(mb);
            nvb = new Ms::VBox(score);
            nvb->setBoxHeight(tvb->boxHeight());
            nvb->setBoxWidth(tvb->boxWidth());
            nvb->setTopGap(tvb->topGap());
            nvb->setBottomGap(tvb->bottomGap());
            nvb->setTopMargin(tvb->topMargin());
            nvb->setBottomMargin(tvb->bottomMargin());
            nvb->setLeftMargin(tvb->leftMargin());
            nvb->setRightMargin(tvb->rightMargin());
            nvb->setAutoSizeEnabled(tvb->isAutoSizeEnabled());
        }
    } else {
        score->setScoreOrder(ScoreOrderConverter::convertScoreOrder(scoreOptions.order));
    }

    score->setName(qtrc("notation", "Untitled"));
    score->setCreated(true);

    score->checkChordList();

    Ms::Fraction timesig(scoreOptions.timesigNumerator, scoreOptions.timesigDenominator);
    score->sigmap()->add(0, timesig);

    Ms::Fraction firstMeasureTicks = pickupMeasure ? Ms::Fraction(scoreOptions.measureTimesigNumerator,
                                                                  scoreOptions.measureTimesigDenominator) : timesig;

    for (int i = 0; i < measures; ++i) {
        Ms::Fraction tick = firstMeasureTicks + timesig * (i - 1);
        if (i == 0) {
            tick = Ms::Fraction(0, 1);
        }
        QList<Ms::Rest*> puRests;
        for (Ms::Score* _score : score->scoreList()) {
            Ms::Rest* rest = 0;
            Ms::Measure* measure = new Ms::Measure(_score);
            measure->setTimesig(timesig);
            measure->setTicks(timesig);
            measure->setTick(tick);

            if (pickupMeasure && tick.isZero()) {
                measure->setIrregular(true);                // don’t count pickup measure
                measure->setTicks(Ms::Fraction(scoreOptions.measureTimesigNumerator,
                                               scoreOptions.measureTimesigDenominator));
            }
            _score->measures()->add(measure);

            for (Ms::Staff* staff : _score->staves()) {
                int staffIdx = staff->idx();
                if (tick.isZero()) {
                    Ms::TimeSig* ts = new Ms::TimeSig(_score);
                    ts->setTrack(staffIdx * VOICES);
                    ts->setSig(timesig, scoreOptions.timesigType);
                    Ms::Measure* m = _score->firstMeasure();
                    Ms::Segment* s = m->getSegment(Ms::SegmentType::TimeSig, Ms::Fraction(0, 1));
                    s->add(ts);
                    Part* part = staff->part();
                    if (!part->instrument()->useDrumset()) {
                        //
                        // transpose key
                        //
                        Ms::KeySigEvent nKey = ks;
                        if (!nKey.custom() && !nKey.isAtonal() && part->instrument()->transpose().chromatic
                            && !score->styleB(Ms::Sid::concertPitch)) {
                            int diff = -part->instrument()->transpose().chromatic;
                            nKey.setKey(Ms::transposeKey(nKey.key(), diff, part->preferSharpFlat()));
                        }
                        // do not create empty keysig unless custom or atonal
                        if (nKey.custom() || nKey.isAtonal() || nKey.key() != Key::C) {
                            staff->setKey(Ms::Fraction(0, 1), nKey);
                            Ms::KeySig* keysig = new Ms::KeySig(score);
                            keysig->setTrack(staffIdx * VOICES);
                            keysig->setKeySigEvent(nKey);
                            Ms::Segment* ss = measure->getSegment(Ms::SegmentType::KeySig, Ms::Fraction(0, 1));
                            ss->add(keysig);
                        }
                    }
                }

                // determined if this staff is linked to previous so we can reuse rests
                bool linkedToPrevious = staffIdx && staff->isLinked(_score->staff(staffIdx - 1));
                if (measure->timesig() != measure->ticks()) {
                    if (!linkedToPrevious) {
                        puRests.clear();
                    }
                    std::vector<Ms::TDuration> dList = toDurationList(measure->ticks(), false);
                    if (!dList.empty()) {
                        Ms::Fraction ltick = tick;
                        int k = 0;
                        foreach (Ms::TDuration d, dList) {
                            if (k < puRests.count()) {
                                rest = static_cast<Ms::Rest*>(puRests[k]->linkedClone());
                            } else {
                                rest = new Ms::Rest(score, d);
                                puRests.append(rest);
                            }
                            rest->setScore(_score);
                            rest->setTicks(d.fraction());
                            rest->setTrack(staffIdx * VOICES);
                            Ms::Segment* seg = measure->getSegment(Ms::SegmentType::ChordRest, ltick);
                            seg->add(rest);
                            ltick += rest->actualTicks();
                            k++;
                        }
                    }
                } else {
                    if (linkedToPrevious && rest) {
                        rest = static_cast<Ms::Rest*>(rest->linkedClone());
                    } else {
                        rest = new Ms::Rest(score, Ms::TDuration(Ms::TDuration::DurationType::V_MEASURE));
                    }
                    rest->setScore(_score);
                    rest->setTicks(measure->ticks());
                    rest->setTrack(staffIdx * VOICES);
                    Ms::Segment* seg = measure->getSegment(Ms::SegmentType::ChordRest, tick);
                    seg->add(rest);
                }
            }
        }
    }

    //
    // select first rest
    //
    Measure* m = score->firstMeasure();
    for (Ms::Segment* s = m->first(); s; s = s->next()) {
        if (s->segmentType() == Ms::SegmentType::ChordRest) {
            if (s->element(0)) {
                score->select(s->element(0), SelectType::SINGLE, 0);
                break;
            }
        }
    }

    {
        QString title = score->metaTag("workTitle");
        QString subtitle = score->metaTag("subtitle");
        QString composer = score->metaTag("composer");
        QString lyricist = score->metaTag("lyricist");

        if (!title.isEmpty() || !subtitle.isEmpty() || !composer.isEmpty() || !lyricist.isEmpty()) {
            Ms::MeasureBase* measure = score->measures()->first();
            if (measure->type() != ElementType::VBOX) {
                Ms::MeasureBase* nm = nvb ? nvb : new Ms::VBox(score);
                nm->setTick(Ms::Fraction(0, 1));
                nm->setNext(measure);
                score->measures()->add(nm);
                measure = nm;
            } else if (nvb) {
                delete nvb;
            }
            if (!title.isEmpty()) {
                Ms::Text* s = new Ms::Text(score, Ms::Tid::TITLE);
                s->setPlainText(title);
                measure->add(s);
            }
            if (!subtitle.isEmpty()) {
                Ms::Text* s = new Ms::Text(score, Ms::Tid::SUBTITLE);
                s->setPlainText(subtitle);
                measure->add(s);
            }
            if (!composer.isEmpty()) {
                Ms::Text* s = new Ms::Text(score, Ms::Tid::COMPOSER);
                s->setPlainText(composer);
                measure->add(s);
            }
            if (!lyricist.isEmpty()) {
                Ms::Text* s = new Ms::Text(score, Ms::Tid::POET);
                s->setPlainText(lyricist);
                measure->add(s);
            }
        } else if (nvb) {
            delete nvb;
        }
    }

    if (scoreOptions.withTempo) {
        Ms::Fraction ts = timesig;

        QString text("<sym>metNoteQuarterUp</sym> = %1");
        double bpm = scoreOptions.tempo.valueBpm;
        switch (ts.denominator()) {
        case 1:
            text = "<sym>metNoteWhole</sym> = %1";
            bpm /= 4;
            break;
        case 2:
            text = "<sym>metNoteHalfUp</sym> = %1";
            bpm /= 2;
            break;
        case 4:
            text = "<sym>metNoteQuarterUp</sym> = %1";
            break;
        case 8:
            if (ts.numerator() % 3 == 0) {
                text = "<sym>metNoteQuarterUp</sym><sym>space</sym><sym>metAugmentationDot</sym> = %1";
                bpm /= 1.5;
            } else {
                text = "<sym>metNote8thUp</sym> = %1";
                bpm *= 2;
            }
            break;
        case 16:
            if (ts.numerator() % 3 == 0) {
                text = "<sym>metNote8thUp</sym><sym>space</sym><sym>metAugmentationDot</sym> = %1";
                bpm *= 1.5;
            } else {
                text = "<sym>metNote16thUp</sym> = %1";
                bpm *= 4;
            }
            break;
        case 32:
            if (ts.numerator() % 3 == 0) {
                text = "<sym>metNote16thUp</sym><sym>space</sym><sym>metAugmentationDot</sym> = %1";
                bpm *= 3;
            } else {
                text = "<sym>metNote32ndUp</sym> = %1";
                bpm *= 8;
            }
            break;
        case 64:
            if (ts.numerator() % 3 == 0) {
                text = "<sym>metNote32ndUp</sym><sym>space</sym><sym>metAugmentationDot</sym> = %1";
                bpm *= 6;
            } else {
                text = "<sym>metNote64thUp</sym> = %1";
                bpm *= 16;
            }
            break;
        default:
            break;
        }

        Ms::TempoText* tt = new Ms::TempoText(score);
        tt->setXmlText(text.arg(bpm));

        double tempo = scoreOptions.tempo.valueBpm;
        tempo /= 60; // bpm -> bps

        tt->setTempo(tempo);
        tt->setFollowText(true);
        tt->setTrack(0);
        Ms::Segment* seg = score->firstMeasure()->first(Ms::SegmentType::ChordRest);
        seg->add(tt);
        score->setTempo(seg, tempo);
    }

    {
        Ms::ScoreLoad sl;
        score->doLayout();
    }

    setScore(score);
    initExcerpts(excerpts);

    score->setExcerptsChanged(true);

    if (!templateScore) {
        parts()->setParts(scoreOptions.parts);
    }

    return make_ret(Err::NoError);
}

mu::RetVal<bool> MasterNotation::created() const
{
    RetVal<bool> result;
    if (!score()) {
        result.ret = make_ret(Err::NoScore);
        return result;
    }

    return RetVal<bool>::make_ok(score()->created());
}

mu::ValNt<bool> MasterNotation::needSave() const
{
    ValNt<bool> needSave;
    needSave.val = !masterScore()->saved();
    needSave.notification = undoStack()->stackChanged();

    return needSave;
}

void MasterNotation::addExcerpts(const ExcerptNotationList& excerpts)
{
    undoStack()->prepareChanges();

    ExcerptNotationList result = m_excerpts.val;
    for (IExcerptNotationPtr excerptNotation : excerpts) {
        auto it = std::find(result.begin(), result.end(), excerptNotation);
        if (it != result.end()) {
            continue;
        }

        ExcerptNotation* excerptNotationImpl = get_impl(excerptNotation);
        if (!excerptNotationImpl->excerpt()) {
            excerptNotationImpl->setExcerpt(new Ms::Excerpt(masterScore()));
        }

        excerptNotationImpl->init();
        result.push_back(excerptNotation);
    }

    undoStack()->commitChanges();

    doSetExcerpts(result);
}

void MasterNotation::removeExcerpts(const ExcerptNotationList& excerpts)
{
    undoStack()->prepareChanges();

    for (IExcerptNotationPtr excerptNotation : excerpts) {
        auto it = std::find(m_excerpts.val.begin(), m_excerpts.val.end(), excerptNotation);
        if (it == m_excerpts.val.end()) {
            continue;
        }

        Ms::Excerpt* excerpt = get_impl(excerptNotation)->excerpt();
        masterScore()->undo(new Ms::RemoveExcerpt(excerpt));
        m_excerpts.val.erase(it);
    }

    undoStack()->commitChanges();

    doSetExcerpts(m_excerpts.val);
}

void MasterNotation::doSetExcerpts(ExcerptNotationList excerpts)
{
    m_excerpts.set(excerpts);
    static_cast<MasterNotationParts*>(m_parts.get())->setExcerpts(excerpts);
}

IExcerptNotationPtr MasterNotation::newExcerptNotation() const
{
    return std::make_shared<ExcerptNotation>();
}

mu::ValCh<ExcerptNotationList> MasterNotation::excerpts() const
{
    return m_excerpts;
}

INotationPartsPtr MasterNotation::parts() const
{
    return m_parts;
}

ExcerptNotationList MasterNotation::potentialExcerpts() const
{
    auto excerptExists = [this](const Ms::InstrumentList* content) {
        for (const Ms::Excerpt* excerpt : masterScore()->excerpts()) {
            for (const Part* part : excerpt->parts()) {
                const Ms::InstrumentList* partInstruments = part->instruments();

                if (partInstruments && (*partInstruments == *content)) {
                    return true;
                }
            }
        }

        return false;
    };

    QList<Part*> parts;
    for (Part* part : score()->parts()) {
        const Ms::InstrumentList* content = part->instruments();

        if (content && !excerptExists(content)) {
            parts << part;
        }
    }

    QList<Ms::Excerpt*> excerpts = Ms::Excerpt::createExcerptsFromParts(parts);

    ExcerptNotationList result;

    for (Ms::Excerpt* excerpt : excerpts) {
        IExcerptNotationPtr excerptNotation = std::make_shared<ExcerptNotation>(excerpt);
        result.push_back(excerptNotation);
    }

    return result;
}

void MasterNotation::onSaveCopy()
{
    score()->setCreated(false);
    undoStack()->stackChanged().notify();
}

void MasterNotation::initExcerpts(const QList<Ms::Excerpt*>& excerpts)
{
    ExcerptNotationList notationExcerpts;

    for (Ms::Excerpt* excerpt : excerpts) {
        IExcerptNotationPtr excerptNotation = std::make_shared<ExcerptNotation>(excerpt);

        get_impl(excerptNotation)->init();
        excerptNotation->notation()->setOpened(true);

        notationExcerpts.push_back(excerptNotation);
    }

    doSetExcerpts(notationExcerpts);
}
