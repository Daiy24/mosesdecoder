#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>

#include "moses/Util.h"
#include "InputFileStream.h"
#include "OutputFileStream.h"
#include "psd/FeatureExtractor.h"
#include "psd/FeatureConsumer.h"
#include "RuleTable.h"
#include "InputTreeRep.h"
#include "PsdLine.h"

using namespace std;
using namespace Moses;
using namespace PSD;

void WritePhraseIndex(const TargetIndexType &index, const string &outFile)
{
  OutputFileStream out(outFile);
  if (! out.good()) {
    cerr << "error: Failed to open " << outFile << endl;
    CHECK(false);
  }
  TargetIndexType::right_map::const_iterator it; // keys are sorted in the map
  for (it = index.right.begin(); it != index.right.end(); it++)
    out << it->second << "\n";
  out.Close();
}

ContextType ReadFactoredLine(const string &line, size_t factorCount)
{
  ContextType out;
  vector<string> words = Tokenize(line, " ");
  vector<string>::const_iterator it;
  for (it = words.begin(); it != words.end(); it++) {
    vector<string> factors = Tokenize(*it, "|");
    if (factors.size() != factorCount) {
      cerr << "error: Wrong count of factors: " << *it << endl;
      exit(1);
    }
    out.push_back(factors);
  }
  return out;
}

size_t GetSizeOfSentence(const string &line)
{
	vector<string> words = Tokenize(line, " ");
	return words.size();
}

inline int makeSpanInterval(int span)
{
    switch(span)
    {
        case 1: return 1;
        case 2: return 2;
        case 3: return 3;
        case 4: return 4;
        case 5: return 4;
        case 6: return 4;
        case 7: return 7;
        case 8: return 7;
        case 9: return 7;
        case 10: return 7;
        default: return 8;
    }

}

int main(int argc, char**argv)
{

  std::cerr << "Beginning Extraction of Syntactic Features for LHS of rules..." << std::endl;

  if (argc != 8) {
    cerr << "error: wrong arguments" << endl;
    cerr << "Usage: extract-psd psd-file parsed-file corpus phrase-table extractor-config output-train output-index" << endl;
    exit(1);
  }
  InputFileStream psd(argv[1]);
  if (! psd.good()) {
    cerr << "error: Failed to open " << argv[1] << endl;
    exit(1);
  }
  InputFileStream corpus(argv[2]);
  if (! corpus.good()) {
    cerr << "error: Failed to open " << argv[2] << endl;
    exit(1);
  }
  InputFileStream parse(argv[3]);
  if (! corpus.good()) {
    cerr << "error: Failed to open " << argv[3] << endl;
    exit(1);
  }

  RuleTable rtable(argv[4]);
  ExtractorConfig config;
  config.Load(argv[5]);
  FeatureExtractor extractor(rtable.GetTargetIndex(), config, true);
  VWFileTrainConsumer consumer(argv[6]);
  WritePhraseIndex(rtable.GetTargetIndex(), argv[7]);

  // one source phrase can have multiple correct translations
  // these will be on consecutive lines in the input PSD file
  string srcPhrase = "";
  ContextType context;
  vector<float> losses;

  //TODO : Uncomment when debugging
  //Fabienne Braune : vector for holding p(e|f)
  //vector<float> pEgivenF;

  vector<string> syntFeats;
  vector<ChartTranslation> translations;
  vector<SyntaxLabel> syntLabels;
  SyntaxLabel parentLabel("NOTAG",true);
  string span;
  size_t spanStart = 0;
  size_t spanEnd = 0;
  size_t sentID = 0;
  size_t srcTotal = 0;
  size_t tgtTotal = 0;
  size_t srcSurvived = 0;
  size_t tgtSurvived = 0;


  //get span of non-terminals in rule
  vector<string> rhsSourceToken;
  vector<string> sourceSentToken;
  Tokenize(rhsSourceToken,srcPhrase," ");
  vector<pair<unsigned int, unsigned int> > nonTermSpans;

   // don't generate features if no translations survived filtering
   bool hasTranslation = false;

  string corpusLine;
  string rawPSDLine;
  string parseLine;


  while (getline(psd, rawPSDLine)) {
    tgtTotal++;
    PSDLine psdLine(rawPSDLine); // parse one line in PSD file
    //std::cerr << "Found PSD line : " << rawPSDLine << std::endl;

    // get to the current sentence in annotated corpus
    while (psdLine.GetSentID() > sentID) {
      getline(corpus, corpusLine);
      getline(parse, parseLine);
      sentID++;
    }

   //std::cerr << "Looking for PSD line : " << psdLine.GetSrcPhrase()<< " : " << psdLine.GetTgtPhrase() << std::endl;

    if (! rtable.SrcExists(psdLine.GetSrcPhrase())) {
      //std::cout << "Source not found, continue" << std::endl;
      continue;
    }
    // we have all correct translations of the current phrase
    if (psdLine.GetSrcPhrase() != srcPhrase) {
      // generate features
      if (hasTranslation) { //ignore first round
    	//cerr << "EXTRACTING FEATURES FOR : " << srcPhrase << std::endl;
        srcSurvived++;
        extractor.GenerateFeaturesChart(&consumer, context, srcPhrase, syntFeats, parentLabel.GetString(), span, spanStart, spanEnd, translations, losses);}
        //Fabienne Braune: Uncomment for debugging : pEgivenF is passed with losses to check numbers
        //extractor.GenerateFeaturesChart(&consumer, context, srcPhrase, syntFeats, parentLabel.GetString(), span, spanStart, spanEnd, translations, losses, pEgivenF);}
      // set new source phrase, context, translations and losses
      srcPhrase = psdLine.GetSrcPhrase();
      spanStart = psdLine.GetSrcStart();
      spanEnd = psdLine.GetSrcEnd();
      context = ReadFactoredLine(corpusLine, config.GetFactors().size());
      translations = rtable.GetTranslations(srcPhrase);
      losses.clear();

      //Fabienne Braune : uncomment if need pEgivenF for debugging
      //pEgivenF.clear();
      syntFeats.clear();
      parentLabel.clear();
      losses.resize(translations.size(), 1);
      //Fabienne Braune : uncomment if need pEgivenF for debugging
      //pEgivenF.resize(translations.size(), 1);
      srcTotal++;

      // after extraction, set translation to false again
      hasTranslation = false;

      //get span corresponding to lhs of rule
      int spanInt = (spanEnd - spanStart) + 1;

      CHECK( spanInt > 0);
      stringstream s;
      s << spanInt;
      span = s.str();

        // set new syntax features
        size_t sentSize = GetSizeOfSentence(corpusLine);

        //cerr << "Extracting syntactic features..." << parseLineString << endl;
        Moses::InputTreeRep myInputChart = Moses::InputTreeRep(sentSize);
        myInputChart.Read(parseLine);
        //cerr << "PRINTING CHART ..." << endl;
        //myInputChart.Print(sentSize);

        //get syntax label associated to span
        vector<SyntaxLabel> lhsSyntaxLabels = myInputChart.GetLabels(spanStart, spanEnd);
        //std::cerr << "Getting parent label : " << spanStart << " : " << spanEnd << std::endl;

        bool IsBegin = false;
        string noTag = "NOTAG";
        parentLabel = myInputChart.GetParent(spanStart,spanEnd,IsBegin);
        IsBegin = false;
        while(!parentLabel.GetString().compare("NOTAG"))
        {
            //cerr << "LOOKING FOR PARENT OF : " << parentLabel.GetString() << endl;
            parentLabel = myInputChart.GetParent(spanStart,spanEnd,IsBegin);
            //cerr << "FOUND PARENT : " << parentLabel.GetString() << endl;
            //cerr << "BEGIN OF CHART : " << IsBegin << endl;
            if( !(IsBegin ) )
            {spanStart--;}
            else
            {spanEnd++;}
        }

        //cerr << "FOUND PARENT LABEL : " << parentLabel.GetString() << endl;

        //iterate over labels and get strings
        //MAYBE INEFFICIENT

        vector<SyntaxLabel>::iterator itr_syn_lab;
        for(itr_syn_lab = lhsSyntaxLabels.begin(); itr_syn_lab != lhsSyntaxLabels.end(); itr_syn_lab++)
        {
            SyntaxLabel syntaxLabel = *itr_syn_lab;
            //std::cerr << "Found Syntax Label : " << syntaxLabel.GetString() << std::endl;
            CHECK(syntaxLabel.IsNonTerm() == 1);
            string syntFeat = syntaxLabel.GetString();

            bool toRemove = false;
            if( (lhsSyntaxLabels.size() > 1 ) && !(syntFeat.compare( myInputChart.GetNoTag() )) )
            {toRemove = true;}

            if(toRemove == false)
            {
                syntFeats.push_back(syntFeat);
            }
        }

    //restore span start and span end for extraction of context and bow features
    spanStart = psdLine.GetSrcStart();
    spanEnd = psdLine.GetSrcEnd();

    bool foundTgt = false;
    size_t tgtPhraseID = rtable.GetTgtPhraseID(psdLine.GetTgtPhrase(), &foundTgt);
    //cerr << "PSD LINE SEARCHED FOR X: " << psdLine.GetSrcPhrase() << " : " << psdLine.GetTgtPhrase() << "X: Found ? : " << foundTgt << std::endl;

    //condition that target must be found has to be enforced

    //TODO : Not robust : shall crash if no target is found
    //if it is not found, no example should be generated
		if (foundTgt) {
		  // addadd correct translation (i.e., set its loss to 0)
		  for (size_t i = 0; i < translations.size(); i++) {
			if (translations[i].m_index == tgtPhraseID) {
				//std::cerr << "ID for target found : " << tgtPhraseID << " : " << translations[i].m_index << std::endl;
				losses[i] = 0;
				 //Fabienne Braune : uncomment if need pEgivenF for debugging
				//pEgivenF[i] = 0;
				hasTranslation = true;
				tgtSurvived++;
			  	break;
			}
			else
			{
				//std::cerr << "ID for target not found : " << tgtPhraseID << " : " << translations[i].m_index << std::endl;
			}
		  }
		}
    }
  }
   // generate features for the last source phrase
  if (hasTranslation) {
    srcSurvived++;
    //Fabienne Braune: Uncomment for debugging : pEgivenF is passed with losses to check numbers
    //extractor.GenerateFeaturesChart(&consumer, context, srcPhrase, syntFeats, parentLabel.GetString(), span, spanStart, spanEnd, translations, losses, pEgivenF);
    extractor.GenerateFeaturesChart(&consumer, context, srcPhrase, syntFeats, parentLabel.GetString(), span, spanStart, spanEnd, translations, losses);
  }

    // output statistics about filtering
  cerr << "Filtered phrases: source " << srcTotal - srcSurvived << ", target " << tgtTotal - tgtSurvived << endl;
  cerr << "Remaining phrases: source " << srcSurvived << ", target " << tgtSurvived << endl;

  // flush FeatureConsumer
  consumer.Finish();
}