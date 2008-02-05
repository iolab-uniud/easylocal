
header {
#include "EasyLocal.h"
#include <iostream>
#include <fstream>
#include <sstream>
}
options {
    language = "Cpp";
    namespaceAntlr = "antlr";
    namespaceStd = "std";
}

/** This is the parser specification for the ExpSpec language for
    batch file processing.
    It is written in the ANTLR Meta-Language to be output as a C++
    file.
    @author Luca Di Gaspero (digasper@dimi.uniud.it)
*/

class ExpSpecParser extends Parser;

options {
    k = 2;
    defaultErrorHandler = false;
}

tokens {
    NATURAL; INTEGER; REAL;
}

experiments
{ 
    easylocal::curr_line = 0;    
}
    : (singleSolve SEMI! | trialSolve SEMI!)*
    ;

protected singleSolve
{ 
    easylocal::AbstractSolver* sol; 
    std::string output_filename = "";
    std::ostream *logstream = &std::cout;
    std::ostream *plotstream = NULL;
    std::ostream *resstream = NULL;
    std::string plot_filename = "";    
    unsigned int solve_timeout = 0;
}
    : sol=solveExpression
        ("options" BBLOCK! (solveOptions[output_filename,logstream,
                    resstream,plot_filename,solve_timeout])* EBLOCK!)?
        
        { 
            try
            {
                easylocal::SolverStatisticsBox ssb;
                *logstream << easylocal::Chronometer::Now() 
                           << "SINGLE SOLVE" << std::endl;
                *logstream << "SOLVER SETTINGS: " << std::endl;
                sol->Print(*logstream);
                sol->AttachLogStream(logstream);
                if (plot_filename != "")
                {
                  plotstream = new std::ofstream(plot_filename.c_str());
                  if (!*plotstream)
                    throw easylocal::std::logic_exception("Cannot open file " + plot_filename + " for plotting");
                }
                sol->AttachPlotStream(plotstream);
                sol->SetTimeout(solve_timeout);
                sol->Solve();
                ssb = sol->GetStatistics();
                sol->DetachLogStream();                
                sol->DetachPlotStream();
                if (resstream != NULL)
                {
                    *resstream << "SINGLE SOLVE " << sol->GetName() << std::endl;
                    *resstream << sol->GetInstanceName() << std::endl;
                    *resstream << "Time\tCost Components [violations] (objective)" << std::endl;
                    *resstream << "----------------------------------------------------------------------------" << std::endl;
                    ssb.Print(*resstream);
                }                    
                if (plotstream != NULL)
                  delete plotstream;
            } 
            catch (easylocal::std::logic_exception& e)
            {
                if (logstream != &std::cout)
                  delete logstream;
                 throw antlr::SemanticException(e.toString(),
                    this->getFilename(),
                    easylocal::curr_line, 0); 
                exit(-1);
            }
        }
    ;

protected trialSolve
{ 
    easylocal::AbstractSolver* sol; 
    std::string output_filename = "";
    std::string plot_filename = "";
    std::ostream *logstream = &std::cout;
    std::ostream *plotstream = NULL;
    std::ostream *resstream = NULL;
    unsigned int n, s = 0, solve_timeout = 0;
}
    : "try" n=expression "times"
        ("or" "for" "at" "most" s=expression "seconds")?
        sol=solveExpression
        ("options" BBLOCK! (solveOptions[output_filename,logstream,
                    resstream,plot_filename,solve_timeout])* EBLOCK!)?
        
        { 
            double total_time = 0.0;
            try
            {               
                easylocal::MultiTrialStatisticsBox mtsb;
                mtsb.trial.resize(n);
                *logstream << easylocal::Chronometer::Now() 
                           << "MULTI TRIAL SOLVE" << std::endl;
                *logstream << "SOLVER SETTINGS: " << std::endl;
                sol->Print(*logstream);
                *logstream << "NUMBER OF TRIALS: " << n << std::endl;
                if (s != 0)
                  *logstream << "UPPER BOUND ON TIME: " << s << "secs" << std::endl;
                if (resstream != NULL)
                {
                    *resstream << easylocal::Chronometer::Now() << "MULTI TRIAL SOLVE " << sol->GetName() << std::endl;                
                    *resstream << sol->GetInstanceName() << std::endl;
                    *resstream << "Trial\t\tAcc. Time\tTime\tCost Components [violations] (objective)" << std::endl;
                    *resstream << "----------------------------------------------------------------------------" << std::endl;
                }
                for (unsigned int c = 0; c < 72; c++)
                  *logstream << '-';	
                *logstream << std::endl;
                sol->AttachLogStream(logstream);                
                for (unsigned int i = 0; i < n; i++)
                {
                    *logstream << "TRIAL [" << i+1 << "]" << std::endl;
                    if (plot_filename != "")
                    {
                        std::ostringstream tmp;
                        tmp << plot_filename << "-";
                        tmp.flags(std::ios::right);
                        tmp.fill('0');
                        tmp.width((unsigned int)std::log10((float)n)+1);
                        tmp << i << ".plot";	 
                        plotstream = new std::ofstream(tmp.str().c_str());
                        if (!*plotstream)
                          throw easylocal::std::logic_exception("Cannot open file " + tmp.str() + " for plotting");
                        sol->AttachPlotStream(plotstream);
                        *logstream << "PLOTTING RESULTS ON FILE " << tmp.str() << std::endl;
                    }     
                    for (unsigned int c = 0; c < 72; c++)
                      *logstream << '-';
                    *logstream << std::endl;
                    sol->SetTimeout(solve_timeout);
                    sol->Solve();                    
                    if (output_filename != "")
                    {	    
                        std::ostringstream tmp;
                        tmp << output_filename << "-";
                        tmp.flags(std::ios::right);
                        tmp.fill('0');
                        tmp.width((unsigned int)std::log10((float)n)+1);
                        tmp << i+1 << ".out";	    
                        sol->Save(tmp.str());                        
                    }	                   
                    const easylocal::SolverStatisticsBox& ssb = sol->GetStatistics();

                    total_time += ssb.GetTime();
                    mtsb.trial[i] = ssb;
                    if (resstream != NULL)
                    {
                        *resstream << "TRIAL [" << i+1 << "]\t" << total_time << "\t";
                        ssb.Print(*resstream); 
                    }                    
                    for (unsigned int c = 0; c < 72; c++)
                      *logstream << '-';	
                    *logstream << std::endl;
                    if (plotstream != NULL)
                      delete plotstream;
                    sol->DetachPlotStream();       
                    if (s != 0 && total_time > s)
                      break;
                }
                sol->DetachLogStream();		
                *logstream << "MULTI TRIAL SOLVE SUMMARY: " << std::endl;
                mtsb.Print(*logstream);
                for (unsigned int c = 0; c < 72; c++)
                  *logstream << '-';
                *logstream << std::endl;             
            } 
            catch (easylocal::std::logic_exception& e)
            {
                if (logstream != &std::cout)
                  delete logstream;              
                throw antlr::SemanticException(e.toString(),
                    this->getFilename(),
                    easylocal::curr_line, 0); 
            }
        }
    ;

protected solveExpression returns [easylocal::AbstractSolver* sol]
{
    std::string inst;  
}
    : "solve"^ inst=instance "using"! sol=solver
        { 
            sol->Load(inst); 
            sol->ClearMovers(); 
        }              
        moversToApply[sol]         
    ;

protected instance returns [std::string s]
{ 
    unsigned int val, val2; 
    std::ostringstream oss("");
}
    : "instance" id:IDENT 
        { oss << id->getText(); }
        (LPAREN! val=expression { oss << " " << val; }
            (COMMA! val2=expression { oss << " " << val2; })* RPAREN!)?
        { s = oss.str(); }
    ;

protected solver returns [easylocal::AbstractSolver* sol]
{ easylocal::EasyLocalObject* obj = NULL; }
    : obj=qualident
        { sol = dynamic_cast<easylocal::AbstractSolver*>(obj); }
    ;

protected qualident returns [easylocal::EasyLocalObject* obj]
{ easylocal::ParametersList pars;}
    : id:IDENT^ LPAREN! pars=parameterList RPAREN!
        { 
            try 
            {
                obj = easylocal::EasyLocalSystemObjects::Lookup(id->getText()); 
            }
            catch (easylocal::ObjectNotFoundException& ex)
            {
                throw antlr::SemanticException("Object " + id->getText() 
                    + " cannot be found",
                    this->getFilename(),
                    easylocal::curr_line, 0); 
                obj = NULL;
            }
            if (obj != NULL && pars.size() > 0)
            obj->SetParameters(pars);
        }
    ;

protected parameterList returns [easylocal::ParametersList pars]
{ pars.clear(); }
    : /* empty parameter: treated as void */
    | "void"^
    | { easylocal::ParameterBind pb, oth_pb; } 
        pb=parameter { pars.push_back(pb); } 
        (COMMA! oth_pb=parameter 
            { pars.push_back(oth_pb); }
        )*
    ;

protected parameter returns [easylocal::ParameterBind bind]
    : id:IDENT { bind.name = id->getText(); } EQUAL^
        (st_val:STRING
            { bind.type = "string"; bind.value = st_val->getText(); }
        | nat_val:NATURAL
            { bind.type = "natural"; bind.value = nat_val->getText(); }
        | int_val:INTEGER
            { bind.type = "integer"; bind.value = int_val->getText(); }
        | real_val:REAL
            { bind.type = "real"; bind.value = real_val->getText(); }
        | patt_val:PATTERN
            { bind.type = "pattern"; bind.value = patt_val->getText(); }
        )
    ;

protected moversToApply[easylocal::AbstractSolver* sol]
    : moverList[sol]
    ;

protected moverList[easylocal::AbstractSolver* sol]
    : /* empty mover list: nothing to do */
    | mover[sol] (AMPERSAND! mover[sol])*
    ;

protected mover[easylocal::AbstractSolver* sol]
{ easylocal::EasyLocalObject* obj = NULL; }
    : obj=qualident
        { 
            easylocal::AbstractMover* mvr = dynamic_cast<easylocal::AbstractMover*>(obj);
            if (mvr != NULL)
            sol->AddMover(mvr); 
            else
            throw antlr::SemanticException("Object " + obj->GetName() + " is not a valid mover ",
                this->getFilename(),
                easylocal::curr_line, 0);
        }
    ;

protected solveOptions[std::string& output_filename, std::ostream*& logstream,
    std::ostream*& resstream, std::string& plot_filename, unsigned int& timeout]
    : "output" EQUAL^ out_name:STRING SEMI!
        { output_filename = out_name->getText(); }
    | "log" EQUAL^ log_name:STRING SEMI!
        { 
            if (logstream != &std::cout)
              delete logstream;
            logstream = new std::ofstream(log_name->getText().c_str(), std::ios::out|std::ios::app); 
        }
    | "results" EQUAL^ res_name:STRING SEMI!
        { 
            if (resstream != NULL)
                delete resstream;
            resstream = new std::ofstream(res_name->getText().c_str(), std::ios::out|std::ios::app);
        }
    | "plot" EQUAL^ plot_name:STRING SEMI!
        { plot_filename = plot_name->getText(); }
    | "trial_timeout" EQUAL^ timeout=expression SEMI!
    ;

protected expression returns [unsigned int val]
    : n:NATURAL
        { val = (unsigned int)atoi(n->getText().c_str()); }
    ;

class ExpSpecLexer extends Lexer;
options {
    k = 4;
    charVocabulary = '\3'..'\377';
    filter = IGNORE;
}

protected IGNORE 
    : WS | SL_COMMENT | ML_COMMENT 
    ;

protected WS 
    : (' '
        |  '\t' { tab(); }
        |  '\n' { easylocal::curr_line++; newline();  }
        |  '\r')
        { $setType(antlr::Token::SKIP); }
    ;

protected SL_COMMENT 
    : "//" (~'\n')* '\n'
        { $setType(antlr::Token::SKIP); easylocal::curr_line++; newline(); }
	;

protected ML_COMMENT
    : "/*"
        ( {LA(2) != '/'}? '*'
        | '\n' { easylocal::curr_line++; newline(); }
        | ~('*'|'\n')
        )*
        "*/"
        { $setType(antlr::Token::SKIP); }
    ;   

EQUAL 
options {
    paraphrase = "=";
}
    : '='
    ;

SEMI
options {
    paraphrase = ";";
}
    :';'
    ;


COMMA
options {
    paraphrase = ",";
}
    :','
    ;

IDENT
options {
    testLiterals = true;
    paraphrase = "an identifer";
} 
    : ('a'..'z'|'A'..'Z'|'_') ('a'..'z'|'A'..'Z'|'_'|'-'|'0'..'9'|'.')*
    ;

STRING
    : '"'! (~'"')* '"'!
    ;

LPAREN
options {
    paraphrase="(";
}
    : '('
    ;

RPAREN
options {
    paraphrase=")";
}
    : ')'
    ;

BBLOCK
options {
    paraphrase="{";
}
    : '{'
    ;


EBLOCK
options {
    paraphrase="}";
}
    : '}'
    ;

AMPERSAND
options {
  paraphrase="&";
}
    : '&'
    ;

protected DIGIT
    : '0'..'9'
    ;

NUMBER
    : {LA(1) == '.' || LA(2) == '.'}? ('0')? '.' (DIGIT)+ (('E'|'e') ('+'|'-')?(DIGIT)+)? {$setType(REAL);}
    | (DIGIT)+ {$setType(NATURAL);} ('.' (DIGIT)+ {$setType(REAL);})?
    ;

SIGNED_NUMBER
    : ('+'|'-') n:NUMBER {if (n->getType() == NATURAL) $setType(INTEGER);}
    ;

PATTERN
    : '['! ('1' | '2')+ ']'!
    ;
